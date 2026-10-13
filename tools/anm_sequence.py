"""Measure sampled dense ANM geometry and continuity; no playback or loop inference."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import sys

if __package__:
    from .anm_inspect import inspect_anm
    from .anm_pose import evaluate_pose, influences, packed_quaternion
    from .ktmdl_inspect import inspect_skeleton, read_model
else:
    from anm_inspect import inspect_anm
    from anm_pose import evaluate_pose, influences, packed_quaternion
    from ktmdl_inspect import inspect_skeleton, read_model

MAX_SAMPLES = 512
MAX_VERTEX_SAMPLES = 2_000_000
MAX_TRIANGLE_SAMPLES = 5_000_000


def rotation_distance(a, b):
    """Quaternion q and -q represent the same rotation."""
    dot = min(1.,abs(sum(x*y for x,y in zip(a,b))))
    return math.degrees(2*math.acos(dot))


def duplicate_groups(model, metadata, weight_order):
    positions,bindings = {},{}
    for p,s in zip(metadata["packets"],metadata["skeletal_metadata"]["packets"],strict=True):
        attr = next(a for a in p["attributes"] if a["semantic"] == 16)
        for i,raw in enumerate(s["raw_influences"]):
            xyz = struct.unpack_from("<3f",model,p["vertex_data_offset"]+i*p["vertex_stride"]+attr["offset"])
            key = p["index"],i
            positions.setdefault(xyz,[]).append(key)
            merged = {}
            for bone,weight in influences(raw,s["palette"],weight_order):
                merged[bone] = merged.get(bone,0.)+weight
            binding = tuple(sorted(merged.items()))
            bindings.setdefault((xyz,binding),[]).append(key)
    return ([g for g in positions.values() if len(g)>1],
            [g for g in bindings.values() if len(g)>1])


def group_gap(packets, groups):
    maximum = 0.
    for group in groups:
        packet,vertex = group[0]
        anchor = packets[packet]["positions"][vertex]
        for packet,vertex in group[1:]:
            maximum = max(maximum,math.dist(anchor,packets[packet]["positions"][vertex]))
    return maximum


def vertex_step(before, after):
    result = {"distance":0.,"packet":None,"vertex":None}
    for pi,(p,q) in enumerate(zip(before,after,strict=True)):
        for vi,(v,w) in enumerate(zip(p["positions"],q["positions"],strict=True)):
            distance = math.dist(v,w)
            if distance > result["distance"]:
                result = {"distance":distance,"packet":pi,"vertex":vi}
    return result


def measure_sequence(model: bytes, animation: bytes, *, core_offset: int = 0,
                     sequence_offset: int | None = None, start: int = 0,
                     end: int | None = None, weight_order: str) -> dict:
    clip = inspect_anm(animation,core_offset=core_offset,sequence_offset=sequence_offset)
    end = clip["last_sample_index"] if end is None else end
    if type(start) is not int or type(end) is not int or not 0 <= start <= end < clip["sample_count"]:
        raise ValueError("sequence range must contain valid inclusive integer samples")
    count = end-start+1
    if count > MAX_SAMPLES:
        raise ValueError("sequence sample limit exceeded; select a smaller range")
    metadata = inspect_skeleton(model)
    triangles = sum(p["triangle_count_without_repeated_indices"] for p in metadata["packets"])
    if count*metadata["vertex_count"] > MAX_VERTEX_SAMPLES or count*triangles > MAX_TRIANGLE_SAMPLES:
        raise ValueError("sequence geometry work limit exceeded; select a smaller range")
    groups,same_bindings = duplicate_groups(model,metadata,weight_order)
    rotations = [t for t in clip["tracks"] if t["curve_type"] == 0x1c]
    frames = []
    first_packets = first_joints = None
    previous_packets = previous_joints = previous_rotations = None
    for sample in range(start,end+1):
        pose,mesh = evaluate_pose(model,animation,sample=sample,core_offset=core_offset,
                                 sequence_offset=sequence_offset,weight_order=weight_order)
        packets = mesh["packets"]
        joints = [m[12:15] for m in pose["global_bone_matrices"]]
        quaternions = [packed_quaternion(animation[t["value_offset"]+sample*6:t["value_offset"]+(sample+1)*6]) for t in rotations]
        digest = hashlib.sha256()
        min_area = math.inf
        zero_faces = 0
        for p in packets:
            for v in p["positions"]:
                digest.update(struct.pack("<3d",*v))
            for a,b,c in p["triangles"]:
                v,w,x = [p["positions"][i] for i in (a,b,c)]
                u,t = [w[i]-v[i] for i in range(3)],[x[i]-v[i] for i in range(3)]
                cross = [u[1]*t[2]-u[2]*t[1],u[2]*t[0]-u[0]*t[2],u[0]*t[1]-u[1]*t[0]]
                area = math.sqrt(sum(x*x for x in cross))*.5
                zero_faces += area == 0
                min_area = min(min_area,area)
        frame = {"sample":sample,"seconds":pose["sample_seconds"],
                 "bounds_min":pose["bounds_min"],"bounds_max":pose["bounds_max"],
                 "position_sha256_f64le":digest.hexdigest(),"root_origins":[joints[i] for i in metadata["skeletal_metadata"]["roots"]],
                 "triangle_area_min":min_area if min_area != math.inf else None,
                 "zero_area_triangles":zero_faces,
                 "duplicate_position_anchor_gap_max":group_gap(packets,groups),
                 "identical_binding_anchor_gap_max":group_gap(packets,same_bindings),
                 "vertex_step_max":None,"joint_step_max":None,"local_rotation_step_degrees_max":None}
        if previous_packets is not None:
            frame["vertex_step_max"] = vertex_step(previous_packets,packets)
            distances = [math.dist(v,w) for v,w in zip(previous_joints,joints,strict=True)]
            node = max(range(len(distances)),key=distances.__getitem__)
            frame["joint_step_max"] = {"distance":distances[node],"node":node}
            angles = [rotation_distance(v,w) for v,w in zip(previous_rotations,quaternions,strict=True)]
            index = max(range(len(angles)),key=angles.__getitem__)
            frame["local_rotation_step_degrees_max"] = {"degrees":angles[index],"node":rotations[index]["node"]}
        if first_packets is None:
            first_packets,first_joints = packets,joints
        previous_packets,previous_joints,previous_rotations = packets,joints,quaternions
        frames.append(frame)
    return {"schema_version":1,"model_sha256":metadata["sha256"],"animation_sha256":clip["sha256"],
            "weight_order":weight_order,"start_sample":start,"end_sample":end,"evaluated_samples":count,
            "fps":clip["fps"],"duration_seconds":(end-start)/clip["fps"] if clip["fps"] else None,
            "vertex_count":metadata["vertex_count"],"triangle_count":triangles,
            "duplicate_position_groups":len(groups),"identical_binding_groups":len(same_bindings),
            "bounds_min":[min(f["bounds_min"][c] for f in frames) for c in range(3)],
            "bounds_max":[max(f["bounds_max"][c] for f in frames) for c in range(3)],
            "first_last_vertex_distance_max":vertex_step(first_packets,previous_packets),
            "first_last_joint_distance_max":max(math.dist(v,w) for v,w in zip(first_joints,previous_joints,strict=True)),
            "frames":frames,
            "limitations":"Measurements of integer samples in source units, not automatic smoothness approval. "
                          "Duplicate gaps are distances from each group's first vertex, not pairwise diameters. "
                          "Separate surfaces can have different bindings. No interpolation, loop inference, events or playback."}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model",required=True,type=Path)
    parser.add_argument("--animation",required=True,type=Path)
    parser.add_argument("--core-offset",type=int,default=0)
    parser.add_argument("--sequence-offset",type=int)
    parser.add_argument("--start",type=int,default=0)
    parser.add_argument("--end",type=int,help="inclusive; default last dense sample")
    parser.add_argument("--weight-order",required=True,choices=("first","last"))
    parser.add_argument("--json",type=Path,help="new report path; default stdout")
    args = parser.parse_args(argv)
    try:
        if args.json and (args.json.exists() or args.json.is_symlink()):
            raise ValueError("output already exists")
        report = measure_sequence(read_model(args.model),read_model(args.animation),core_offset=args.core_offset,
                                  sequence_offset=args.sequence_offset,start=args.start,end=args.end,weight_order=args.weight_order)
        text = json.dumps(report,indent=2,allow_nan=False)+"\n"
        if args.json:
            with args.json.open("x",encoding="utf-8") as f:
                f.write(text)
        else:
            print(text,end="")
        return 0
    except (OSError,ValueError) as error:
        print(f"anm_sequence: {error}",file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
