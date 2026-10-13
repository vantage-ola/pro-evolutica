"""Extract a flat DXT1 DDS texture from the observed PES bundle/WE00 layout."""

from __future__ import annotations

import argparse
import binascii
import hashlib
import json
import os
from pathlib import Path
import stat
import struct
import sys
import zlib

MAX_INPUT_SIZE = 64 * 1024**2
MAX_ENTRIES = 4096
MAX_DIMENSION = 4096
MAX_PIXELS = 4 * 1024**2


def inspect_dds(data: bytes) -> dict:
    if len(data) < 128 or data[:4] != b"DDS ":
        raise ValueError("missing or truncated DDS header")
    header = struct.unpack_from("<31I", data, 4)
    if header[0] != 124 or header[18] != 32:
        raise ValueError("invalid DDS header or pixel-format size")
    height, width, depth, mip_count = header[2], header[3], header[5], header[6] or 1
    if not 1 <= width <= MAX_DIMENSION or not 1 <= height <= MAX_DIMENSION or width * height > MAX_PIXELS:
        raise ValueError("DDS dimensions exceed the supported limits")
    if depth > 1 or header[27] != 0:
        raise ValueError("only flat 2D DDS textures are supported; no volume or cube maps")
    if not header[19] & 4 or data[84:88] != b"DXT1":
        raise ValueError("only DXT1 DDS textures are supported")
    if mip_count > max(width, height).bit_length():
        raise ValueError("DDS mip count exceeds the possible chain")
    levels, offset = [], 128
    for level in range(mip_count):
        w, h = max(1, width >> level), max(1, height >> level)
        size = max(1, (w + 3) // 4) * max(1, (h + 3) // 4) * 8
        levels.append({"level": level, "width": w, "height": h, "offset": offset, "size_bytes": size})
        offset += size
    if len(data) != offset:
        raise ValueError(f"DDS mip data length mismatch: expected {offset} bytes, got {len(data)}")
    return {"width": width, "height": height, "pixel_format": "DXT1", "mip_count": mip_count,
            "size_bytes": len(data), "sha256": hashlib.sha256(data).hexdigest(), "levels": levels}


def extract_texture(data: bytes, index: int = 0) -> tuple[bytes, dict]:
    if len(data) > MAX_INPUT_SIZE:
        raise ValueError(f"input exceeds the {MAX_INPUT_SIZE}-byte limit")
    if len(data) < 8:
        raise ValueError("truncated PES bundle header")
    count, table_offset = struct.unpack_from("<II", data)
    if not 1 <= count <= MAX_ENTRIES or table_offset != 8:
        raise ValueError("unsupported PES bundle count or table offset")
    table_end = table_offset + count * 12
    if table_end > len(data):
        raise ValueError("truncated PES bundle table")
    if not 0 <= index < count:
        raise ValueError(f"texture index must be between 0 and {count - 1}")
    entries = []
    for i in range(count):
        offset, size, unknown = struct.unpack_from("<III", data, table_offset + i * 12)
        if offset > len(data) or offset + size > len(data) or (size and offset < table_end):
            raise ValueError(f"bundle entry {i} is out of bounds or overlaps the table")
        entries.append({"index": i, "offset": offset, "size_bytes": size, "unknown_u32": unknown})
    entry = entries[index]
    start, size = entry["offset"], entry["size_bytes"]
    if size < 16 or data[start:start + 4] != b"WE00":
        raise ValueError(f"bundle entry {index} is not a supported WE00 texture")
    dds_size = struct.unpack_from("<I", data, start + 8)[0]
    if dds_size > size - 16:
        raise ValueError("WE00 DDS length exceeds the bundle entry")
    dds_start, dds_end = start + 16, start + 16 + dds_size
    padding = size - 16 - dds_size
    if padding > 15 or any(data[dds_end:start + size]):
        raise ValueError("unsupported WE00 trailing data")
    dds = data[dds_start:dds_end]
    metadata = inspect_dds(dds)
    return dds, {"schema_version": 1, "format": "PES-bundle/WE00/DDS",
                 "input_size_bytes": len(data), "input_sha256": hashlib.sha256(data).hexdigest(),
                 "entry_count": count, "entry": entry, "we00_header_hex": data[start:start + 16].hex(),
                 "dds_offset": dds_start, "dds": metadata}


def _rgb565(value: int) -> tuple[int, int, int, int]:
    r, g, b = (value >> 11) & 31, (value >> 5) & 63, value & 31
    return (r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2), 255


def decode_dxt1(data: bytes) -> tuple[int, int, bytes]:
    """Decode only the highest-resolution level, including DXT1 one-bit alpha."""
    info = inspect_dds(data)
    width, height = info["width"], info["height"]
    rgba = bytearray(width * height * 4)
    offset = 128
    for y in range(0, height, 4):
        for x in range(0, width, 4):
            c0, c1, selectors = struct.unpack_from("<HHI", data, offset)
            offset += 8
            a, b = _rgb565(c0), _rgb565(c1)
            if c0 > c1:
                colors = [a, b, tuple((2 * a[i] + b[i] + 1) // 3 for i in range(3)) + (255,),
                          tuple((a[i] + 2 * b[i] + 1) // 3 for i in range(3)) + (255,)]
            else:
                colors = [a, b, tuple((a[i] + b[i]) // 2 for i in range(3)) + (255,), (0, 0, 0, 0)]
            for row in range(4):
                for col in range(4):
                    if x + col < width and y + row < height:
                        pixel = ((y + row) * width + x + col) * 4
                        selector = (selectors >> (2 * (row * 4 + col))) & 3
                        rgba[pixel:pixel + 4] = bytes(colors[selector])
    return width, height, bytes(rgba)


def dxt1_png(data: bytes) -> bytes:
    width, height, rgba = decode_dxt1(data)
    def chunk(kind: bytes, payload: bytes) -> bytes:
        return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", binascii.crc32(kind + payload))
    rows = b"".join(b"\0" + rgba[y * width * 4:(y + 1) * width * 4] for y in range(height))
    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b"")


def inspect_texture(path: Path | str, *, index: int = 0,
                    output: Path | str | None = None, png: Path | str | None = None) -> dict:
    path = Path(path)
    destinations = [Path(p) for p in (output, png) if p is not None]
    if len({p.resolve() for p in destinations}) != len(destinations):
        raise ValueError("DDS and PNG output paths must differ")
    for destination in destinations:
        if destination.exists() or destination.is_symlink():
            raise ValueError(f"output already exists: {destination}")
    fd = os.open(path, os.O_RDONLY | os.O_NONBLOCK | os.O_NOFOLLOW)
    with os.fdopen(fd, "rb") as source:
        before = os.fstat(source.fileno())
        if not stat.S_ISREG(before.st_mode):
            raise ValueError("input must be a regular file")
        if before.st_size > MAX_INPUT_SIZE:
            raise ValueError(f"input exceeds the {MAX_INPUT_SIZE}-byte limit")
        data = source.read(before.st_size + 1)
        after = os.fstat(source.fileno())
        if (before.st_size, before.st_mtime_ns, before.st_ctime_ns) != (
                after.st_size, after.st_mtime_ns, after.st_ctime_ns) or len(data) != before.st_size:
            raise ValueError("input changed during reading; retry with a stable file")
    dds, report = extract_texture(data, index)
    preview = dxt1_png(dds) if png is not None else None
    report["file_name"] = path.name
    if output is not None:
        with Path(output).open("xb") as destination:
            destination.write(dds)
        report["dds_output_name"] = Path(output).name
    if png is not None:
        with Path(png).open("xb") as destination:
            destination.write(preview)
        report["png"] = {"output_name": Path(png).name, "size_bytes": len(preview),
                         "sha256": hashlib.sha256(preview).hexdigest()}
    return report


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path, help="decoded PES bundle")
    parser.add_argument("--index", type=int, default=0, help="zero-based bundle entry; default: 0")
    parser.add_argument("--output", type=Path, help="new DDS path")
    parser.add_argument("--png", type=Path, help="new PNG preview of the highest-resolution level")
    parser.add_argument("--json", type=Path, help="new report path; default: stdout")
    args = parser.parse_args(argv)
    try:
        destinations = [p for p in (args.output, args.png, args.json) if p is not None]
        if len({p.resolve() for p in destinations}) != len(destinations):
            raise ValueError("output paths must differ")
        for destination in destinations:
            if destination.exists() or destination.is_symlink():
                raise ValueError(f"output already exists: {destination}")
            if not destination.parent.is_dir():
                raise ValueError(f"output directory does not exist: {destination.parent}")
        report = inspect_texture(args.input, index=args.index, output=args.output, png=args.png)
        encoded = json.dumps(report, indent=2, ensure_ascii=True) + "\n"
        if args.json:
            with args.json.open("x", encoding="utf-8") as destination:
                destination.write(encoded)
        else:
            print(encoded, end="")
        return 0
    except (OSError, ValueError) as exc:
        print(f"pes_texture: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
