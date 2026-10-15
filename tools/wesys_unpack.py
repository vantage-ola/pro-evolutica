"""Decode observed stored and zlib WESYS wrappers; never execute their payloads."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import stat
import struct
import sys
import zlib

MAX_INPUT_SIZE = 64 * 1024**2
MAX_DECODED_SIZE = 64 * 1024**2
MAX_PADDING = 15
VERSION = b"\x00\x01\x01"
STORED_VERSION = b"\x00\x01\x00"
HEADER_SIZE = 16


def decode_wesys(data: bytes) -> tuple[bytes, dict]:
    """Decode one complete stream with explicit input and output limits."""
    if len(data) > MAX_INPUT_SIZE:
        raise ValueError(f"input exceeds the {MAX_INPUT_SIZE}-byte limit")
    if len(data) < HEADER_SIZE:
        raise ValueError("truncated WESYS header")
    if data[3:8] != b"WESYS":
        raise ValueError("not a WESYS payload")
    version = data[:3]
    if version not in (VERSION, STORED_VERSION):
        raise ValueError(f"unsupported WESYS version: {version.hex()}")
    payload_size, size_field = struct.unpack_from("<II", data, 8)
    stored = version == STORED_VERSION
    if stored and size_field:
        raise ValueError("unsupported stored WESYS header (expected zero at offset 12)")
    if payload_size > MAX_INPUT_SIZE - HEADER_SIZE:
        raise ValueError("declared payload size exceeds the input limit" if stored else
                         "declared compressed size exceeds the input limit")
    decoded_size = payload_size if stored else size_field
    if decoded_size > MAX_DECODED_SIZE:
        raise ValueError(f"decoded size exceeds the {MAX_DECODED_SIZE}-byte limit")
    end = HEADER_SIZE + payload_size
    if end > len(data):
        raise ValueError("truncated WESYS payload")
    padding_size = len(data) - end
    if padding_size > MAX_PADDING or any(data[end:]):
        raise ValueError("unsupported trailing data (only up to 15 zero padding bytes are accepted)")
    if stored:
        decoded = data[HEADER_SIZE:end]
    else:
        decoder = zlib.decompressobj()
        try:
            # The extra byte detects a false size declaration without unbounded inflate.
            decoded = decoder.decompress(memoryview(data)[HEADER_SIZE:end], decoded_size + 1)
        except zlib.error as exc:
            raise ValueError(f"invalid WESYS zlib stream: {exc}") from exc
        if len(decoded) > decoded_size or decoder.unconsumed_tail:
            raise ValueError("decoded payload exceeds its declared size")
        if not decoder.eof:
            raise ValueError("incomplete WESYS zlib stream")
        if decoder.unused_data:
            raise ValueError("declared compressed payload contains data after the zlib stream")
        if len(decoded) != decoded_size:
            raise ValueError("decoded payload length differs from its declared size")
    report = {"schema_version": 1, "format": "WESYS", "version_hex": version.hex(),
              "compression": "none" if stored else "zlib", "payload_size_bytes": payload_size,
              "input_size_bytes": len(data), "input_sha256": hashlib.sha256(data).hexdigest(),
              "compressed_size_bytes": 0 if stored else payload_size, "padding_size_bytes": padding_size,
              "decoded_size_bytes": len(decoded),
              "decoded_sha256": hashlib.sha256(decoded).hexdigest(),
              "decoded_magic_hex": decoded[:32].hex()}
    return decoded, report


def inspect_wesys(path: Path | str, *, output: Path | str | None = None) -> dict:
    path = Path(path)
    if output is not None:
        output = Path(output)
        if output.exists() or output.is_symlink():
            raise ValueError(f"output already exists: {output}")
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
    decoded, report = decode_wesys(data)
    report["file_name"] = path.name
    if output is not None:
        with output.open("xb") as destination:
            destination.write(decoded)
        report["output_name"] = output.name
    return report


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path, help="raw extracted WESYS entry")
    parser.add_argument("--output", type=Path, help="new decoded file; omit to inspect only")
    parser.add_argument("--json", type=Path, help="new JSON report; default: stdout")
    args = parser.parse_args(argv)
    try:
        destinations = [p for p in (args.output, args.json) if p is not None]
        if len({p.resolve() for p in destinations}) != len(destinations):
            raise ValueError("decoded output and JSON report must differ")
        for destination in destinations:
            if destination.exists() or destination.is_symlink():
                raise ValueError(f"output already exists: {destination}")
            if not destination.parent.is_dir():
                raise ValueError(f"output directory does not exist: {destination.parent}")
        report = inspect_wesys(args.input, output=args.output)
        data = json.dumps(report, indent=2, ensure_ascii=True) + "\n"
        if args.json:
            with args.json.open("x", encoding="utf-8") as stream:
                stream.write(data)
        else:
            print(data, end="")
        return 0
    except (OSError, ValueError) as exc:
        print(f"wesys_unpack: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
