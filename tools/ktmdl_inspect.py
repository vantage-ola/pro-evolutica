"""Inspect observed little-endian KTMDL geometry and optional skeletal metadata."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import stat
import struct
import sys

MAX_INPUT_SIZE = 64 * 1024**2
MAX_RECORDS = 4096
MAX_VERTICES = 1_000_000
MAX_INDICES = 2_000_000
HEADER_SIZE = 192


def inspect_model(data: bytes) -> dict:
    if len(data) > MAX_INPUT_SIZE:
        raise ValueError("KTMDL exceeds the input limit")
    if len(data) < HEADER_SIZE or data[:8] != b"KTMDL\0\0\0":
        raise ValueError("missing or truncated standalone KTMDL header")
    major, minor = struct.unpack_from("<II", data, 8)
    if (major, minor) not in ((2, 1), (2, 2)) or data[16:18] != b"\x01\x00":
        raise ValueError("only observed little-endian KTMDL 2.1/2.2 is supported")
    if struct.unpack_from("<I", data, 144)[0] != len(data):
        raise ValueError("KTMDL declared size differs from input length")

    def region(offset: int, size: int) -> None:
        if offset < HEADER_SIZE or size < 0 or offset + size > len(data):
            raise ValueError("KTMDL region is outside the payload or overlaps its header")

    def table(field: int, stride: int) -> tuple[int, int]:
        count, offset = struct.unpack_from("<Ii", data, field)
        if count > MAX_RECORDS:
            raise ValueError("KTMDL table exceeds record limit")
        if count:
            region(offset, count * stride)
        return count, offset

    packet_count, packet_base = table(40, 96)
    stream_count, stream_base = table(48, 32)
    element_count, element_base = table(136, 4)
    if not packet_count or not stream_count or not element_count:
        raise ValueError("KTMDL has no supported geometry tables")
    data_size, data_base = struct.unpack_from("<Ii", data, 116)
    region(data_base, data_size)
    ranges = sorted([(packet_base, packet_base + packet_count * 96),
                     (stream_base, stream_base + stream_count * 32),
                     (element_base, element_base + element_count * 4),
                     (data_base, data_base + data_size)])
    if any(left[1] > right[0] for left, right in zip(ranges, ranges[1:])):
        raise ValueError("KTMDL geometry tables/data overlap")
    packets = []
    total_vertices = total_indices = 0

    def stream(base: int, expected_type: int) -> tuple[int, int, int, int, int]:
        if base < stream_base or (base - stream_base) % 32 or base + 32 > stream_base + stream_count * 32:
            raise ValueError("packet stream reference is outside the stream table")
        relative, count = struct.unpack_from("<iI", data, base)
        stride, elements = data[base + 9:base + 11]
        declaration = base + struct.unpack_from("<i", data, base + 12)[0]
        if struct.unpack_from("<i", data, base + 20)[0] != expected_type:
            raise ValueError("unexpected KTMDL stream type")
        limit = MAX_VERTICES if expected_type == 0 else MAX_INDICES
        if not 0 < count <= limit:
            raise ValueError("KTMDL stream count exceeds supported limits")
        start = base + relative
        size = count * (stride if expected_type == 0 else 2)
        if start < data_base or start + size > data_base + data_size:
            raise ValueError("stream data is outside the declared data region")
        if expected_type == 0:
            if not stride or not elements:
                raise ValueError("empty vertex stride or declaration")
            if declaration < element_base or (declaration - element_base) % 4 or \
                    declaration + elements * 4 > element_base + element_count * 4:
                raise ValueError("vertex declaration is outside the element table")
        return start, count, stride, declaration, elements

    for index in range(packet_count):
        packet = packet_base + index * 96
        if data[packet + 2] != 0:
            raise ValueError("only observed triangle-strip packets are supported")
        vertex_streams, vertex_relative = struct.unpack_from("<Ii", data, packet + 16)
        index_streams, index_relative = struct.unpack_from("<Ii", data, packet + 32)
        if vertex_streams != 1 or index_streams != 1:
            raise ValueError("only one vertex and one index stream per packet are supported")
        start, vertices, stride, declaration, elements = stream(packet + vertex_relative, 0)
        indices_start, index_count, _, _, _ = stream(packet + index_relative, 1)
        total_vertices += vertices
        total_indices += index_count
        if total_vertices > MAX_VERTICES or total_indices > MAX_INDICES:
            raise ValueError("aggregate KTMDL geometry exceeds limits")
        attributes = []
        position = None
        seen = set()
        sizes = {0: 4, 1: 8, 2: 12, 11: 4}
        for element in range(elements):
            channel, offset, kind, semantic = data[declaration + element * 4:declaration + element * 4 + 4]
            if channel != 0 or kind not in sizes:
                raise ValueError("unsupported vertex channel or element type")
            if offset + sizes[kind] > stride or semantic in seen:
                raise ValueError("vertex element exceeds stride or repeats a semantic")
            seen.add(semantic)
            attributes.append({"offset": offset, "type": kind, "semantic": semantic})
            if semantic == 16:
                if kind != 2:
                    raise ValueError("position must be three float32 values")
                position = offset
        if position is None:
            raise ValueError("vertex declaration has no position")
        lower, upper = [math.inf] * 3, [-math.inf] * 3
        for vertex in range(vertices):
            values = struct.unpack_from("<3f", data, start + vertex * stride + position)
            if not all(math.isfinite(value) for value in values):
                raise ValueError("non-finite KTMDL vertex position")
            for axis, value in enumerate(values):
                lower[axis] = min(lower[axis], value)
                upper[axis] = max(upper[axis], value)
        triangles = restarts = repeated = 0
        previous = []
        for (value,) in struct.iter_unpack("<H", data[indices_start:indices_start + index_count * 2]):
            if value == 65535:
                previous.clear()
                restarts += 1
                continue
            if value >= vertices:
                raise ValueError("KTMDL index exceeds vertex count")
            previous.append(value)
            if len(previous) == 3:
                if len(set(previous)) == 3:
                    triangles += 1
                else:
                    repeated += 1
                previous.pop(0)
        packets.append({"index": index, "primitive": "triangle_strip", "vertex_count": vertices,
                        "index_count": index_count, "vertex_stride": stride, "attributes": attributes,
                        "packet_offset": packet, "vertex_data_offset": start,
                        "index_data_offset": indices_start,
                        "position_bounds_min": lower, "position_bounds_max": upper,
                        "triangle_count_without_repeated_indices": triangles,
                        "repeated_index_windows": repeated, "restart_count": restarts})
    return {"schema_version": 1, "format": "KTMDL", "version": [major, minor],
            "byte_order": "little", "size_bytes": len(data), "sha256": hashlib.sha256(data).hexdigest(),
            "packet_count": packet_count, "vertex_count": total_vertices, "index_count": total_indices,
            "packets": packets,
            "limitations": "Geometry tables/positions/indices only; materials, bones, normals, UV values, "
                           "triangle area, winding and animation are not validated."}


def read_model(path: Path | str) -> bytes:
    path = Path(path)
    fd = os.open(path, os.O_RDONLY | os.O_NONBLOCK | os.O_NOFOLLOW)
    with os.fdopen(fd, "rb") as source:
        before = os.fstat(source.fileno())
        if not stat.S_ISREG(before.st_mode) or before.st_size > MAX_INPUT_SIZE:
            raise ValueError("KTMDL input must be a regular file within the input limit")
        data = source.read(before.st_size + 1)
        after = os.fstat(source.fileno())
        if (before.st_size, before.st_mtime_ns, before.st_ctime_ns) != \
                (after.st_size, after.st_mtime_ns, after.st_ctime_ns) or len(data) != before.st_size:
            raise ValueError("KTMDL input changed while reading")
    return data


def inspect_skeleton(data: bytes) -> dict:
    """Preserve observed bone matrices and raw influences without choosing weight order."""
    report = inspect_model(data)
    if report["vertex_count"] > 65_535:
        raise ValueError("skeletal inspection exceeds the vertex limit")
    tables, ranges = {}, []
    for field, stride in ((24,176), (40,96), (48,32), (56,80),
                          (64,48), (72,160), (80,80), (88,48), (124,16), (136,4)):
        count, base = struct.unpack_from("<Ii", data, field)
        if count > MAX_RECORDS or (count and (base < HEADER_SIZE or base + count*stride > len(data))):
            raise ValueError("skeletal metadata table is outside supported bounds")
        if count:
            ranges.append((base,base+count*stride))
        tables[field] = count,base
    size, base = struct.unpack_from("<Ii", data, 116)
    ranges.append((base,base+size))
    # In the selected skinned sample the count field is 1 despite 19 indices.
    # Support its shared region up to the packet table; do not invent a stride.
    palette_count,palette_base = struct.unpack_from("<Ii",data,32)
    palette_end = tables[40][1]
    if palette_count != 1 or palette_base < HEADER_SIZE or not palette_base < palette_end:
        raise ValueError("only the observed shared bone palette region is supported")
    ranges.append((palette_base,palette_end))
    ranges.sort()
    if any(a[1] > b[0] for a,b in zip(ranges,ranges[1:])):
        raise ValueError("skeletal metadata tables/data overlap")
    count, base = tables[24]
    if not 1 <= count <= 256:
        raise ValueError("skeletal inspection requires 1..256 bones")
    bones = []
    for i in range(count):
        offset = base+i*176
        a = list(struct.unpack_from("<16f",data,offset+16))
        b = list(struct.unpack_from("<16f",data,offset+80))
        if not all(math.isfinite(v) for v in a+b):
            raise ValueError("non-finite bone matrix")
        parent = struct.unpack_from("<i",data,offset+172)[0]
        if parent < -1 or parent >= count:
            raise ValueError("bone parent is out of range")
        error = max(abs(sum(a[r*4+k]*b[k*4+c] for k in range(4)) - (r == c))
                    for r in range(4) for c in range(4))
        bones.append({"index":i,"name_id":struct.unpack_from("<Q",data,offset)[0],
                      "parent":parent,"matrix_a":a,"matrix_b":b,"a_times_b_identity_error":error})
    for bone in bones:
        seen, node = set(),bone["index"]
        while node != -1:
            if node in seen:
                raise ValueError("cyclic bone hierarchy")
            seen.add(node)
            node = bones[node]["parent"]
    packets = []
    for p in report["packets"]:
        packet = p["packet_offset"]
        n, relative = struct.unpack_from("<Ii",data,packet+24)
        start = packet+relative
        if n > 256 or (n and (start < palette_base or (start-palette_base)%2 or
                             start+n*2 > palette_end)):
            raise ValueError("bone palette is outside its table")
        palette = list(struct.unpack_from(f"<{n}H",data,start)) if n else []
        if any(v >= count for v in palette):
            raise ValueError("bone palette references an unknown bone")
        attributes = {a["semantic"]:a for a in p["attributes"]}
        weight, indices = attributes.get(32),attributes.get(33)
        if weight and (weight["type"] not in (0,1,2) or not indices):
            raise ValueError("unsupported blend weights or missing blend indices")
        if indices and (indices["type"] != 11 or not palette):
            raise ValueError("blend indices require UBYTE4 and a bone palette")
        spans = sorted((a["offset"],a["offset"]+{0:4,1:8,2:12,11:4}[a["type"]])
                       for a in p["attributes"])
        if any(a[1] > b[0] for a,b in zip(spans,spans[1:])):
            raise ValueError("skeletal vertex attributes overlap")
        raw = []
        for v in range(p["vertex_count"]) if indices else ():
            offset = p["vertex_data_offset"]+v*p["vertex_stride"]
            weights = list(struct.unpack_from(f"<{weight['type']+1}f",data,offset+weight["offset"])) if weight else []
            if any(not math.isfinite(w) or w < 0 or w > 1 for w in weights) or sum(weights) > 1+2e-6:
                raise ValueError("invalid explicit blend weights")
            slots = list(struct.unpack_from("<4B",data,offset+indices["offset"]))
            if any(s >= n for s in slots[:len(weights)+1]):
                raise ValueError("blend index exceeds the packet palette")
            raw.append({"explicit_weights":weights,"palette_indices":slots})
        packets.append({"index":p["index"],"palette":palette,"raw_influences":raw})
    report["skeletal_metadata"] = {"bones":bones,"roots":[b["index"] for b in bones if b["parent"] == -1],
                                   "packets":packets,"weight_order":"unresolved"}
    report["limitations"] = ("Geometry positions/indices and skeletal hierarchy/raw matrices/palette/weight ranges only. "
                             "Materials, other vertex values, triangle area/winding, matrix space, residual-weight slot, "
                             "deformation and animation remain unverified. The palette region uses the observed shared layout.")
    return report


def inspect_ktmdl(path: Path | str, *, skeleton: bool = False) -> dict:
    data = read_model(path)
    report = inspect_skeleton(data) if skeleton else inspect_model(data)
    report["file_name"] = Path(path).name
    return report


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path, help="standalone extracted .ktmdl")
    parser.add_argument("--json", type=Path, help="new report path; default stdout")
    parser.add_argument("--skeleton", action="store_true", help="validate bones and preserve raw influences")
    args = parser.parse_args(argv)
    try:
        if args.json and (args.json.exists() or args.json.is_symlink()):
            raise ValueError("output already exists")
        report = inspect_ktmdl(args.input, skeleton=args.skeleton)
        output = json.dumps(report, indent=2, allow_nan=False) + "\n"
        if args.json:
            with args.json.open("x", encoding="utf-8") as destination:
                destination.write(output)
        else:
            print(output, end="")
        return 0
    except (OSError, ValueError) as error:
        print(f"ktmdl_inspect: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
