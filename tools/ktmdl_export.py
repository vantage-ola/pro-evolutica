"""Export one supported static KTMDL packet to OBJ for local research."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import sys

if __package__:
    from .ktmdl_inspect import HEADER_SIZE, MAX_RECORDS, inspect_model, read_model
else:
    from ktmdl_inspect import HEADER_SIZE, MAX_RECORDS, inspect_model, read_model

MAX_EXPORT_VERTICES = 65_535
MAX_EXPORT_INDICES = 200_000
IDENTITY = (1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1.)


def strip_triangles(indices: list[int]) -> list[list[int]]:
    """Degenerate windows consume parity; restart markers reset it."""
    faces, window = [], []
    parity = 0
    for index in indices:
        if index == 65535:
            window.clear()
            parity = 0
            continue
        window.append(index)
        if len(window) == 3:
            a, b, c = window
            if a != b and a != c and b != c:
                faces.append([b, a, c] if parity else [a, b, c])
            parity ^= 1
            window.pop(0)
    return faces


def decode_static_model(data: bytes) -> tuple[dict, dict]:
    report = inspect_model(data)
    if report["packet_count"] != 1:
        raise ValueError("static export supports exactly one packet")
    p = report["packets"][0]
    if p["vertex_count"] > MAX_EXPORT_VERTICES or p["index_count"] > MAX_EXPORT_INDICES:
        raise ValueError("static export geometry exceeds limits")
    if any(struct.unpack_from("<I", data, f)[0] for f in (104, 132, 148)):
        raise ValueError("morphs and extra records are not supported by static export")

    def region(offset: int, size: int) -> None:
        if offset < HEADER_SIZE or size < 0 or offset + size > len(data):
            raise ValueError("static metadata region is outside the model")

    # Validate all known fixed tables together, including those retained only as
    # metadata. This prevents references from reinterpreting vertex bytes as names.
    tables, ranges = {}, []
    for field, stride in ((24,176), (32,2), (40,96), (48,32), (56,80),
                          (64,48), (72,160), (80,80), (88,48), (124,16), (136,4)):
        count, offset = struct.unpack_from("<Ii", data, field)
        if count > MAX_RECORDS:
            raise ValueError("static metadata table exceeds record limit")
        if count:
            region(offset, count * stride)
            ranges.append((offset, offset + count * stride))
        tables[field] = count, offset
    data_size, data_base = struct.unpack_from("<Ii", data, 116)
    ranges.append((data_base, data_base + data_size))
    ranges.sort()
    if any(a[1] > b[0] for a, b in zip(ranges, ranges[1:])):
        raise ValueError("static metadata tables/data overlap")

    bone_count, bone_base = tables[24]
    if bone_count > 1:
        raise ValueError("only no bones or one identity root bone is supported")
    if bone_count:
        matrices = [struct.unpack_from("<16f", data, bone_base + o) for o in (16,80)]
        if any(m != IDENTITY for m in matrices) or struct.unpack_from("<i", data, bone_base + 172)[0] != -1:
            raise ValueError("non-identity or parented bones require transform/skin support")
    packet = p["packet_offset"]
    palette_count, palette_rel = struct.unpack_from("<Ii", data, packet + 24)
    palette_size, palette_base = tables[32]
    if palette_count:
        offset = packet + palette_rel
        if bone_count != 1 or palette_count != 1 or offset < palette_base or \
                (offset - palette_base) % 2 or offset + 2 > palette_base + palette_size * 2:
            raise ValueError("unsupported bone palette")
        if struct.unpack_from("<H", data, offset)[0] != 0:
            raise ValueError("bone palette references an unsupported bone")

    decoded = {}
    for attr in p["attributes"]:
        semantic, kind = attr["semantic"], attr["type"]
        if semantic in (32,33):
            raise ValueError("skinned vertex attributes are not supported")
        fmt = {0:"f", 1:"2f", 2:"3f", 11:"4B"}[kind]
        values = [list(struct.unpack_from("<" + fmt, data, p["vertex_data_offset"] +
                    i * p["vertex_stride"] + attr["offset"])) for i in range(p["vertex_count"])]
        if not all(math.isfinite(x) for value in values for x in value):
            raise ValueError("non-finite static vertex attribute")
        decoded[semantic] = values
    types = {a["semantic"]: a["type"] for a in p["attributes"]}
    if types.get(18) != 2 or types.get(22) != 1:
        raise ValueError("static export requires float3 normals and float2 UV0")
    normals = decoded[18]
    lengths = [math.sqrt(sum(x*x for x in n)) for n in normals]
    if min(lengths) == 0:
        raise ValueError("zero-length vertex normal")
    indices = [x[0] for x in struct.iter_unpack("<H", data[p["index_data_offset"]:
                                              p["index_data_offset"] + p["index_count"] * 2])]
    faces = strip_triangles(indices)
    if not faces:
        raise ValueError("static model has no non-degenerate triangles")
    areas, directions = [], [0,0,0]
    positions = decoded[16]
    for a,b,c in faces:
        e = [positions[b][i] - positions[a][i] for i in range(3)]
        f = [positions[c][i] - positions[a][i] for i in range(3)]
        cross = [e[1]*f[2]-e[2]*f[1], e[2]*f[0]-e[0]*f[2], e[0]*f[1]-e[1]*f[0]]
        area = math.sqrt(sum(x*x for x in cross)) * .5
        if area == 0:
            raise ValueError("zero-area triangle with distinct indices")
        areas.append(area)
        dot = sum(cross[i] * (normals[a][i] + normals[b][i] + normals[c][i]) for i in range(3))
        directions[0 if dot > 0 else 1 if dot < 0 else 2] += 1

    material_count, _ = tables[72]
    material_index = struct.unpack_from("<H", data, packet + 4)[0]
    if material_count and material_index >= material_count:
        raise ValueError("packet material reference is out of range")
    name_count, name_base = tables[124]
    type_count, type_base = tables[80]
    names = []
    if name_count:
        debug = struct.unpack_from("<i", data, 96)[0]
        region(debug, 20)
        metadata_end = max(b for a,b in ranges if a != data_base)
        if debug < metadata_end or debug + 20 > data_base:
            raise ValueError("debug names overlap tables/data")
        names_base = debug + struct.unpack_from("<i", data, debug + 4)[0]
        if names_base < debug + 20 or names_base + name_count * 4 > data_base:
            raise ValueError("texture-name table is outside debug metadata")
        for i in range(name_count):
            offset = names_base + struct.unpack_from("<I", data, names_base + i * 4)[0]
            if offset < names_base + name_count * 4 or offset >= data_base:
                raise ValueError("texture name is outside debug metadata")
            end = data.find(b"\0", offset, min(offset + 256, data_base))
            if end < 0:
                raise ValueError("unterminated or overlong texture name")
            try:
                names.append(data[offset:end].decode("utf-8"))
            except UnicodeDecodeError as error:
                raise ValueError("invalid UTF-8 texture name") from error
    ref_count = data[packet + 44]
    if ref_count > 8 or (ref_count and not material_count):
        raise ValueError("unsupported packet texture references")
    refs = []
    for i in range(ref_count):
        ref, parameter, uv = struct.unpack_from("<HBB", data, packet + 48 + i * 4)
        if ref >= type_count:
            raise ValueError("packet texture reference is out of range")
        t = type_base + ref * 80
        mode, name_index = struct.unpack_from("<BH", data, t + 13)
        if name_index >= name_count or types.get(22 + uv) != 1:
            raise ValueError("texture name/UV reference is out of range")
        params = list(struct.unpack_from("<16f", data, t + 16))
        if not all(math.isfinite(x) for x in params):
            raise ValueError("non-finite texture parameter")
        hi, lo = struct.unpack_from("<QQ", data, name_base + name_index * 16)
        refs.append({"binding_index":ref, "mode":mode, "name_index":name_index,
                     "name":names[name_index], "id_hi":hi, "id_lo":lo,
                     "uv_channel":uv, "parameter":parameter, "parameters":params})
    report["static_geometry"] = {"triangle_count":len(faces), "triangle_area_min":min(areas),
        "triangle_area_max":max(areas), "normal_length_min":min(lengths),
        "normal_length_max":max(lengths), "faces_aligned_with_normals":directions[0],
        "faces_opposed_to_normals":directions[1], "faces_perpendicular_to_normals":directions[2],
        "bone_count":bone_count, "bone_transforms":"identity" if bone_count else "none",
        "material_index":material_index, "texture_references":refs,
        "uv0_bounds_min":[min(v[i] for v in decoded[22]) for i in range(2)],
        "uv0_bounds_max":[max(v[i] for v in decoded[22]) for i in range(2)]}
    report["limitations"] = ("One static packet only; positions/UVs retain source axes and units. "
        "No animation, skinning, non-identity bone transforms or material shader reproduction. "
        "Texture IDs are metadata; bundle pairing must be checked separately.")
    return report, {"attributes":decoded, "indices":indices, "triangles":faces}


def obj_text(mesh: dict) -> str:
    lines = ["# Static KTMDL research export: source coordinates and UVs", "o packet_0"]
    for prefix, semantic in (("v",16), ("vt",22), ("vn",18)):
        lines.extend(prefix + " " + " ".join(format(x,".9g") for x in v)
                     for v in mesh["attributes"][semantic])
    for face in mesh["triangles"]:
        lines.append("f " + " ".join(f"{i+1}/{i+1}/{i+1}" for i in face))
    return "\n".join(lines) + "\n"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--obj", required=True, type=Path, help="new local OBJ output")
    parser.add_argument("--json", type=Path, help="new metadata report; default stdout")
    args = parser.parse_args(argv)
    try:
        destinations = [p for p in (args.obj,args.json) if p is not None]
        if len({p.resolve() for p in destinations}) != len(destinations):
            raise ValueError("OBJ and JSON output paths must differ")
        if any(p.exists() or p.is_symlink() for p in destinations):
            raise ValueError("output already exists")
        report, mesh = decode_static_model(read_model(args.input))
        output = obj_text(mesh).encode("utf-8")
        report["file_name"] = args.input.name
        report["obj"] = {"file_name":args.obj.name, "size_bytes":len(output),
                         "sha256":hashlib.sha256(output).hexdigest()}
        with args.obj.open("xb") as dest:
            dest.write(output)
        text = json.dumps(report, indent=2, allow_nan=False) + "\n"
        if args.json:
            with args.json.open("x", encoding="utf-8") as dest:
                dest.write(text)
        else:
            print(text, end="")
        return 0
    except (OSError,ValueError) as error:
        print(f"ktmdl_export: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
