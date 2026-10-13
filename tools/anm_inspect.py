"""Inspect the observed dense PES ANM headers/timing; no quaternion decoding or playback."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import sys

if __package__:
    from .ktmdl_inspect import MAX_INPUT_SIZE, inspect_skeleton, read_model
else:
    from ktmdl_inspect import MAX_INPUT_SIZE, inspect_skeleton, read_model


def inspect_anm(data: bytes, *, core_offset: int = 0, sequence_offset: int | None = None,
                model: bytes | None = None) -> dict:
    if len(data) > MAX_INPUT_SIZE:
        raise ValueError("ANM exceeds the input limit")

    def region(start: int, size: int, end: int = len(data)) -> None:
        if start < 0 or size < 0 or start+size > min(end,len(data)):
            raise ValueError("ANM region is out of bounds")

    region(core_offset,28)
    magic,last,unknown6,unknown8,unknown12,hrel,arel,unknown24 = struct.unpack_from("<IHH5I",data,core_offset)
    if magic != 0xff010001 or last >= 4096:
        raise ValueError("unsupported ANM core or sample count")
    hierarchy,animation = core_offset+hrel,core_offset+arel
    if hrel < 28 or arel <= hrel:
        raise ValueError("ANM chunks overlap the core or are unordered")
    region(hierarchy,16,animation)
    hmagic,count,nrel,endrel = struct.unpack_from("<4I",data,hierarchy)
    if hmagic != 0xff010003 or not 1 <= count <= 128 or nrel < 16 or endrel < nrel+count*2:
        raise ValueError("unsupported ANM hierarchy")
    region(hierarchy,endrel,animation)
    pairs = list(struct.iter_unpack("<Bb",data[hierarchy+nrel:hierarchy+nrel+count*2]))
    if {i for i,p in pairs} != set(range(count)):
        raise ValueError("ANM nodes must have unique contiguous indices")
    parents = dict(pairs)
    for i,parent in pairs:
        if parent < -1 or parent >= count:
            raise ValueError("ANM parent is out of range")
        seen,node = set(),i
        while node != -1:
            if node in seen:
                raise ValueError("cyclic ANM hierarchy")
            seen.add(node)
            node = parents[node]
    region(animation,12)
    amagic,unknown4,table_end = struct.unpack_from("<3I",data,animation)
    if amagic != 0xff010002 or table_end < 16 or (table_end-12)%4 or (table_end-12)//4 > 256:
        raise ValueError("unsupported ANM track table")
    region(animation,table_end)
    offsets = list(struct.unpack_from(f"<{(table_end-12)//4}I",data,animation+12))
    if offsets[-1] != 0 or any(o <= table_end for o in offsets[:-1]) or offsets[:-1] != sorted(set(offsets[:-1])):
        raise ValueError("ANM track offsets must increase and end with one zero")
    starts = [animation+table_end]+[animation+o for o in offsets[:-1]]
    tracks,seen_curves = [],set()
    for i,start in enumerate(starts):
        end = starts[i+1] if i+1 < len(starts) else len(data)
        region(start,16,end)
        curve,mode,samples,node,key_rel,value_rel = struct.unpack_from("<4H2I",data,start)
        if curve not in (0x1c,0x1d) or mode != 3 or samples != last+1 or key_rel or value_rel < 16:
            raise ValueError("only observed dense rotation/FLOAT3 translation tracks are supported")
        if node >= count or (node,curve) in seen_curves or (i == 0 and (node != 0 or curve != 0x1c)):
            raise ValueError("invalid or duplicate ANM node/curve")
        seen_curves.add((node,curve))
        stride = 6 if curve == 0x1c else 12
        value_start = start+value_rel
        region(value_start,samples*stride,end)
        payload = data[value_start:value_start+samples*stride]
        padding = data[value_start+samples*stride:end]
        if len(padding) > 15 or any(padding):
            raise ValueError("unsupported ANM track trailing data")
        track = {"offset":start,"node":node,"curve_type":curve,"storage_mode":mode,
                 "samples":samples,"value_offset":value_start,"value_stride":stride,
                 "payload_sha256":hashlib.sha256(payload).hexdigest()}
        if curve == 0x1d:
            values = list(struct.iter_unpack("<3f",payload))
            if not all(math.isfinite(v) for xyz in values for v in xyz):
                raise ValueError("non-finite ANM translation")
            track["translation_min"] = [min(x[k] for x in values) for k in range(3)]
            track["translation_max"] = [max(x[k] for x in values) for k in range(3)]
        tracks.append(track)

    sequence = None
    if sequence_offset is not None:
        region(sequence_offset,12,core_offset)
        version,bundle,fps,ntracks = struct.unpack_from("<IIHH",data,sequence_offset)
        header_size = 12+ntracks*2
        if not 1 <= fps <= 240 or not 1 <= ntracks <= 32:
            raise ValueError("unsupported ANM sequence FPS or track count")
        region(sequence_offset,header_size,core_offset)
        rels = list(struct.unpack_from(f"<{ntracks}H",data,sequence_offset+12))
        if rels[0] != header_size or rels != sorted(set(rels)):
            raise ValueError("invalid ANM sequence track offsets")
        events = []
        for i,rel in enumerate(rels):
            start = sequence_offset+rel
            end = sequence_offset+rels[i+1] if i+1 < ntracks else core_offset
            region(start,4,end)
            event_rel,channels = struct.unpack_from("<HH",data,start)
            if not 1 <= channels <= 32 or event_rel < 4+channels*2:
                raise ValueError("unsupported ANM sequence channels/event offset")
            region(start,4+channels*2,end)
            counts = list(struct.unpack_from(f"<{channels}H",data,start+4))
            if sum(counts) > 256:
                raise ValueError("ANM sequence event count exceeds limit")
            region(start+event_rel,sum(counts)*16,end)
            raw = [list(struct.unpack_from("<8H",data,start+event_rel+j*16)) for j in range(sum(counts))]
            events.append({"offset":start,"channel_event_counts":counts,"raw_events":raw})
        sequence = {"offset":sequence_offset,"version":version,"bundle_offset":bundle,
                    "fps":fps,"tracks":events}
    report = {"schema_version":1,"format":"PES-ANM-dense","size_bytes":len(data),
              "sha256":hashlib.sha256(data).hexdigest(),"core_offset":core_offset,
              "last_sample_index":last,"sample_count":last+1,
              "core_unknown_fields":[unknown6,unknown8,unknown12,unknown24],
              "hierarchy_offset":hierarchy,"parents":[parents[i] for i in range(count)],
              "animation_offset":animation,"animation_unknown_field":unknown4,
              "tracks":tracks,"sequence":sequence,
              "fps":sequence["fps"] if sequence else None,
              "last_sample_seconds":last/sequence["fps"] if sequence else None,
              "limitations":"Observed dense tracks only. Explicit offsets avoid signature/FPS guesses. "
                             "Packed rotations, transform space, interpolation, event meaning and playback are unverified."}
    if model is not None:
        skeleton = inspect_skeleton(model)["skeletal_metadata"]
        if [b["parent"] for b in skeleton["bones"]] != report["parents"]:
            raise ValueError("ANM hierarchy does not match the supplied KTMDL")
        report["model_sha256"] = hashlib.sha256(model).hexdigest()
        report["model_hierarchy_match"] = True
    return report


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path, help="standalone local ANM chunk")
    parser.add_argument("--core-offset", type=int, default=0)
    parser.add_argument("--sequence-offset", type=int, help="validated sequence header; omit for unknown FPS")
    parser.add_argument("--model", type=Path, help="check hierarchy against a supplied KTMDL")
    parser.add_argument("--json", type=Path, help="new report path; default stdout")
    args = parser.parse_args(argv)
    try:
        if args.json and (args.json.exists() or args.json.is_symlink()):
            raise ValueError("output already exists")
        report = inspect_anm(read_model(args.input), core_offset=args.core_offset,
                             sequence_offset=args.sequence_offset,
                             model=read_model(args.model) if args.model else None)
        report["file_name"] = args.input.name
        text = json.dumps(report,indent=2,allow_nan=False)+"\n"
        if args.json:
            with args.json.open("x",encoding="utf-8") as dest:
                dest.write(text)
        else:
            print(text,end="")
        return 0
    except (OSError,ValueError) as error:
        print(f"anm_inspect: {error}",file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
