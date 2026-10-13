"""Evaluate one dense ANM pose in source coordinates for local research."""

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
    from .ktmdl_inspect import inspect_skeleton, read_model
    from .ktmdl_export import MAX_EXPORT_INDICES, strip_triangles
else:
    from anm_inspect import inspect_anm
    from ktmdl_inspect import inspect_skeleton, read_model
    from ktmdl_export import MAX_EXPORT_INDICES, strip_triangles

RIGID_TOLERANCE = 2e-5


def multiply(a, b):
    """Flat row-vector matrices: point * a * b."""
    return [sum(a[r*4+k]*b[k*4+c] for k in range(4)) for r in range(4) for c in range(4)]


def point(v, matrix):
    return [sum(v[k]*matrix[k*4+c] for k in range(3))+matrix[12+c] for c in range(3)]


def packed_quaternion(payload: bytes) -> list[float]:
    """Observed 48-bit smallest-three packing, returned as x,y,z,w."""
    if len(payload) != 6:
        raise ValueError("packed rotation must contain six bytes")
    bits = int.from_bytes(payload, "little")
    if bits >> 47:
        raise ValueError("unsupported packed rotation reserved bit")
    missing = bits & 3
    components = [((bits >> shift & 32767)-16383.5)/23169.767578125 for shift in (32,17,2)]
    squared = sum(x*x for x in components)
    if squared > 1:
        raise ValueError("packed rotation components exceed unit length")
    components.insert(missing, math.sqrt(1-squared))
    length = math.sqrt(sum(x*x for x in components))
    return [x/length for x in components]


def rotation_matrix(q):
    x,y,z,w = q
    return [1-2*(y*y+z*z), 2*(x*y+z*w), 2*(x*z-y*w), 0.,
            2*(x*y-z*w), 1-2*(x*x+z*z), 2*(y*z+x*w), 0.,
            2*(x*z+y*w), 2*(y*z-x*w), 1-2*(x*x+y*y), 0.,
            0.,0.,0.,1.]


def validate_bind(bones):
    for b in bones:
        for matrix in (b["matrix_a"], b["matrix_b"]):
            affine = max(abs(matrix[k]-(k == 15)) for k in (3,7,11,15))
            orthogonal = max(abs(sum(matrix[r*4+k]*matrix[c*4+k] for k in range(3))-(r == c))
                             for r in range(3) for c in range(3))
            determinant = (matrix[0]*(matrix[5]*matrix[10]-matrix[6]*matrix[9])
                           -matrix[1]*(matrix[4]*matrix[10]-matrix[6]*matrix[8])
                           +matrix[2]*(matrix[4]*matrix[9]-matrix[5]*matrix[8]))
            if max(affine,orthogonal,abs(determinant-1)) > RIGID_TOLERANCE:
                raise ValueError("pose evaluation requires rigid affine bind matrices")
        if b["a_times_b_identity_error"] > RIGID_TOLERANCE:
            raise ValueError("bind matrices are not an inverse pair")


def influences(raw, palette, weight_order):
    if weight_order not in ("first", "last"):
        raise ValueError("weight order must be explicitly first or last")
    explicit = raw["explicit_weights"]
    residual = max(0.,1-sum(explicit))
    weights = ([residual]+explicit) if weight_order == "first" else (explicit+[residual])
    # The inspector validates all used palette slots, including zero weights.
    total = sum(weights)
    return [(palette[index],weight/total) for index,weight in
            zip(raw["palette_indices"],weights) if weight > 0]


def evaluate_pose(model: bytes, animation: bytes | None = None, *, sample: int = 0,
                  core_offset: int = 0, sequence_offset: int | None = None,
                  weight_order: str) -> tuple[dict, dict]:
    if weight_order not in ("first", "last"):
        raise ValueError("weight order must be explicitly first or last")
    metadata = inspect_skeleton(model)
    if metadata["index_count"] > MAX_EXPORT_INDICES:
        raise ValueError("pose geometry exceeds the export index limit")
    skeleton = metadata["skeletal_metadata"]
    bones = skeleton["bones"]
    validate_bind(bones)
    if any(struct.unpack_from("<I",model,f)[0] for f in (104,132)):
        raise ValueError("morphs are unsupported by pose evaluation")
    clip = None
    local = []
    for b in bones:
        parent = b["parent"]
        local.append(multiply(b["matrix_a"],bones[parent]["matrix_b"]) if parent != -1 else list(b["matrix_a"]))
    if animation is not None:
        clip = inspect_anm(animation,core_offset=core_offset,sequence_offset=sequence_offset)
        if clip["parents"] != [b["parent"] for b in bones]:
            raise ValueError("ANM hierarchy does not match the supplied KTMDL")
        if type(sample) is not int or not 0 <= sample < clip["sample_count"]:
            raise ValueError("sample is outside the dense clip")
        rotations = {t["node"]:t for t in clip["tracks"] if t["curve_type"] == 0x1c}
        if set(rotations) != set(range(len(bones))):
            raise ValueError("pose evaluation requires a rotation track for every bone")
        for node,t in rotations.items():
            offset = t["value_offset"]+sample*6
            matrix = rotation_matrix(packed_quaternion(animation[offset:offset+6]))
            matrix[12:15] = local[node][12:15]
            local[node] = matrix
        for t in clip["tracks"]:
            if t["curve_type"] == 0x1d:
                local[t["node"]][12:15] = struct.unpack_from("<3f",animation,t["value_offset"]+sample*12)
    elif sample != 0 or core_offset != 0 or sequence_offset is not None:
        raise ValueError("sample and offset options require an animation")
    global_matrices = {}
    def global_matrix(node):
        if node not in global_matrices:
            parent = bones[node]["parent"]
            global_matrices[node] = multiply(local[node],global_matrix(parent)) if parent != -1 else local[node]
        return global_matrices[node]
    for node in range(len(bones)):
        global_matrix(node)
    skin = [multiply(b["matrix_b"],global_matrices[b["index"]]) for b in bones]
    packets = []
    max_displacement = 0.
    for p,s in zip(metadata["packets"],skeleton["packets"],strict=True):
        if len(s["raw_influences"]) != p["vertex_count"]:
            raise ValueError("every posed vertex requires blend indices")
        attr = next(a for a in p["attributes"] if a["semantic"] == 16)
        positions = []
        for i,raw in enumerate(s["raw_influences"]):
            v = struct.unpack_from("<3f",model,p["vertex_data_offset"]+i*p["vertex_stride"]+attr["offset"])
            moved = [0.,0.,0.]
            for bone,weight in influences(raw,s["palette"],weight_order):
                transformed = point(v,skin[bone])
                for c in range(3):
                    moved[c] += weight*transformed[c]
            if not all(math.isfinite(x) for x in moved):
                raise ValueError("non-finite posed position")
            max_displacement = max(max_displacement,math.dist(v,moved))
            positions.append(moved)
        indices = struct.unpack_from(f'<{p["index_count"]}H',model,p["index_data_offset"])
        packets.append({"positions":positions,"triangles":strip_triangles(indices)})
    all_positions = [v for p in packets for v in p["positions"]]
    report = {"schema_version":1,"model_sha256":metadata["sha256"],
              "animation_sha256":clip["sha256"] if clip else None,
              "sample":sample if clip else None,"fps":clip["fps"] if clip else None,
              "sample_seconds":sample/clip["fps"] if clip and clip["fps"] else None,
              "weight_order":weight_order,"coordinate_space":"source row-vector; no axis or unit conversion",
              "uninterpreted_header_extra_count":struct.unpack_from("<I",model,148)[0],
              "bone_count":len(bones),"packet_count":len(packets),"vertex_count":len(all_positions),
              "triangle_count":sum(len(p["triangles"]) for p in packets),
              "bounds_min":[min(v[c] for v in all_positions) for c in range(3)],
              "bounds_max":[max(v[c] for v in all_positions) for c in range(3)],
              "max_vertex_displacement":max_displacement,
              "global_bone_matrices":[global_matrices[n] for n in range(len(bones))],
              "limitations":"Rigid binds and complete dense rotations only. Explicit residual-weight choice. "
                             "No interpolation, event execution, normals, materials, extra-record interpretation or playback. "
                             "Structural pairing and reference agreement do not prove in-game meaning."}
    return report,{"packets":packets}


def obj_text(mesh):
    lines = ["# Posed KTMDL research export: source coordinates, geometry only"]
    base = 1
    for i,p in enumerate(mesh["packets"]):
        lines.append(f"o packet_{i}")
        lines.extend("v "+" ".join(format(x,".17g") for x in v) for v in p["positions"])
        lines.extend("f "+" ".join(str(base+j) for j in face) for face in p["triangles"])
        base += len(p["positions"])
    return "\n".join(lines)+"\n"


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model",required=True,type=Path)
    parser.add_argument("--animation",type=Path,help="omit to evaluate the bind pose")
    parser.add_argument("--core-offset",type=int,default=0)
    parser.add_argument("--sequence-offset",type=int)
    parser.add_argument("--sample",type=int,default=0,help="integer dense sample; no interpolation")
    parser.add_argument("--weight-order",required=True,choices=("first","last"),help="residual weight slot")
    parser.add_argument("--obj",required=True,type=Path,help="new local geometry-only OBJ")
    parser.add_argument("--json",type=Path,help="new metadata report; default stdout")
    args = parser.parse_args(argv)
    try:
        paths = [p for p in (args.obj,args.json) if p is not None]
        if len({p.resolve() for p in paths}) != len(paths):
            raise ValueError("OBJ and JSON output paths must differ")
        if any(p.exists() or p.is_symlink() for p in paths):
            raise ValueError("output already exists")
        report,mesh = evaluate_pose(read_model(args.model),read_model(args.animation) if args.animation else None,
            sample=args.sample,core_offset=args.core_offset,sequence_offset=args.sequence_offset,weight_order=args.weight_order)
        output = obj_text(mesh).encode("utf-8")
        report["obj"] = {"size_bytes":len(output),"sha256":hashlib.sha256(output).hexdigest()}
        text = json.dumps(report,indent=2,allow_nan=False)+"\n"
        with args.obj.open("xb") as f:
            f.write(output)
        if args.json:
            with args.json.open("x",encoding="utf-8") as f:
                f.write(text)
        else:
            print(text,end="")
        return 0
    except (OSError,ValueError) as error:
        print(f"anm_pose: {error}",file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
