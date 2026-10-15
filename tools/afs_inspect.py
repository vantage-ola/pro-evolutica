"""Inspect an AFS entry table and optionally extract one raw entry by index."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import stat
import struct
import sys
from typing import BinaryIO

MAX_ENTRIES = 100_000
MAX_FILE_SIZE = 8 * 1024**3
MAX_EXTRACT_SIZE = 256 * 1024**2
CHUNK_SIZE = 1024**2


def _read_exact(stream: BinaryIO, size: int) -> bytes:
    data = stream.read(size)
    if len(data) != size:
        raise ValueError("truncated AFS data")
    return data


def _fingerprint(info: os.stat_result) -> tuple[int, int, int]:
    return info.st_size, info.st_mtime_ns, info.st_ctime_ns


def _read_index(stream: BinaryIO, file_size: int) -> list[dict]:
    magic, count = struct.unpack("<4sI", _read_exact(stream, 8))
    if magic != b"AFS\0":
        raise ValueError("not an AFS archive (expected AFS followed by a null byte)")
    if count > MAX_ENTRIES:
        raise ValueError(f"AFS exceeds the {MAX_ENTRIES}-entry limit")
    table_end = 8 + count * 8
    if table_end > file_size:
        raise ValueError("truncated AFS entry table")
    table = _read_exact(stream, count * 8)
    entries = []
    for index, (offset, size) in enumerate(struct.iter_unpack("<II", table)):
        if offset > file_size or offset + size > file_size:
            raise ValueError(f"entry {index} extends beyond the archive")
        if size and offset < table_end:
            raise ValueError(f"entry {index} overlaps the AFS header or entry table")
        entries.append({"index": index, "offset": offset, "size_bytes": size})
    return entries


def read_afs_entry(path: Path | str, entry: int, *,
                   max_size: int = MAX_EXTRACT_SIZE) -> tuple[bytes, dict]:
    """Read one bounded entry after index validation, without hashing the archive."""
    path = Path(path)
    if not isinstance(entry, int) or entry < 0:
        raise ValueError("entry index must be a non-negative integer")
    if not 0 <= max_size <= MAX_EXTRACT_SIZE:
        raise ValueError(f"entry read limit must be between 0 and {MAX_EXTRACT_SIZE}")
    fd = os.open(path, os.O_RDONLY | os.O_NONBLOCK | os.O_NOFOLLOW)
    with os.fdopen(fd, "rb") as stream:
        before = os.fstat(stream.fileno())
        if not stat.S_ISREG(before.st_mode):
            raise ValueError("input must be a regular file")
        if before.st_size > MAX_FILE_SIZE:
            raise ValueError(f"input exceeds the {MAX_FILE_SIZE}-byte limit")
        entries = _read_index(stream, before.st_size)
        if entry >= len(entries):
            raise ValueError(f"entry {entry} is outside the {len(entries)}-entry archive")
        item = entries[entry]
        if not item["size_bytes"]:
            raise ValueError(f"entry {entry} is an empty slot")
        if item["size_bytes"] > max_size:
            raise ValueError(f"entry exceeds the {max_size}-byte read limit")
        stream.seek(item["offset"])
        data = _read_exact(stream, item["size_bytes"])
        if _fingerprint(before) != _fingerprint(os.fstat(stream.fileno())):
            raise ValueError("input changed during reading; retry with a stable file")
    return data, {"schema_version": 1, "format": "AFS-entry", "file_name": path.name,
                  "file_size": before.st_size, "entry_count": len(entries),
                  "entry": item, "entry_sha256": hashlib.sha256(data).hexdigest()}


def _extract(stream: BinaryIO, item: dict, output: Path) -> dict:
    if item["size_bytes"] == 0:
        raise ValueError(f"entry {item['index']} is an empty slot")
    if item["size_bytes"] > MAX_EXTRACT_SIZE:
        raise ValueError(f"entry exceeds the {MAX_EXTRACT_SIZE}-byte extraction limit")
    stream.seek(item["offset"])
    digest = hashlib.sha256()
    with output.open("xb") as destination:
        identity = os.fstat(destination.fileno())
        try:
            remaining = item["size_bytes"]
            while remaining:
                block = _read_exact(stream, min(remaining, CHUNK_SIZE))
                destination.write(block)
                digest.update(block)
                remaining -= len(block)
            destination.flush()
        except BaseException:
            # Remove only the file this call created, even if its path changed.
            try:
                current = output.lstat()
                if (current.st_dev, current.st_ino) == (identity.st_dev, identity.st_ino):
                    output.unlink()
            except OSError:
                pass
            raise
    return {"index": item["index"], "size_bytes": item["size_bytes"],
            "sha256": digest.hexdigest(), "output_name": output.name}


def inspect_afs(path: Path | str, *, entry: int | None = None,
                output: Path | str | None = None) -> dict:
    """Validate the flat AFS index; never interpret names or decode payloads."""
    path = Path(path)
    if (entry is None) != (output is None):
        raise ValueError("entry and output must be supplied together")
    if output is not None:
        output = Path(output)
        if output.exists() or output.is_symlink():
            raise ValueError(f"output already exists: {output}")
    fd = os.open(path, os.O_RDONLY | os.O_NONBLOCK | os.O_NOFOLLOW)
    with os.fdopen(fd, "rb") as stream:
        before = os.fstat(stream.fileno())
        if not stat.S_ISREG(before.st_mode):
            raise ValueError("input must be a regular file")
        if before.st_size > MAX_FILE_SIZE:
            raise ValueError(f"input exceeds the {MAX_FILE_SIZE}-byte limit")
        entries = _read_index(stream, before.st_size)
        count = len(entries)
        if entry is not None:
            if not count:
                raise ValueError("archive has no entries to extract")
            if not 0 <= entry < count:
                raise ValueError(f"entry index must be between 0 and {count - 1}")
        for item in entries:
            stream.seek(item["offset"])
            item["magic_hex"] = _read_exact(stream, min(16, item["size_bytes"])).hex()
        digest = hashlib.sha256()
        stream.seek(0)
        remaining = before.st_size
        while remaining:
            block = _read_exact(stream, min(remaining, CHUNK_SIZE))
            digest.update(block)
            remaining -= len(block)
        if _fingerprint(before) != _fingerprint(os.fstat(stream.fileno())):
            raise ValueError("input changed during inspection; retry with a stable file")
        report = {"schema_version": 1, "format": "AFS", "file_name": path.name,
                  "file_size": before.st_size, "sha256": digest.hexdigest(),
                  "entry_count": count,
                  "empty_count": sum(item["size_bytes"] == 0 for item in entries),
                  "entries": entries}
        if entry is not None:
            report["extraction"] = _extract(stream, entries[entry], output)
            if _fingerprint(before) != _fingerprint(os.fstat(stream.fileno())):
                raise ValueError("input changed during extraction; discard the output and retry")
        return report


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--json", type=Path, help="new report path; default: stdout")
    parser.add_argument("--entry", type=int, help="zero-based entry index to extract")
    parser.add_argument("--output", type=Path, help="new raw output file for --entry")
    args = parser.parse_args(argv)
    if (args.entry is None) != (args.output is None):
        parser.error("--entry and --output must be supplied together")
    try:
        destinations = [p for p in (args.json, args.output) if p is not None]
        if len({p.resolve() for p in destinations}) != len(destinations):
            raise ValueError("JSON report and extracted output must differ")
        for destination in destinations:
            if destination.exists() or destination.is_symlink():
                raise ValueError(f"output already exists: {destination}")
            if not destination.parent.is_dir():
                raise ValueError(f"output directory does not exist: {destination.parent}")
        report = inspect_afs(args.input, entry=args.entry, output=args.output)
        data = json.dumps(report, indent=2, ensure_ascii=True) + "\n"
        if args.json:
            with args.json.open("x", encoding="utf-8") as stream:
                stream.write(data)
        else:
            print(data, end="")
        return 0
    except (OSError, ValueError) as exc:
        print(f"afs_inspect: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
