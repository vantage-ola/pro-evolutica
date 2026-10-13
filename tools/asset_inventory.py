"""Catalogue local files and their first 16 bytes; do not infer asset formats."""

from __future__ import annotations

import argparse
import csv
import json
import os
from pathlib import Path
import stat
import sys

MAGIC_BYTES = 16
MAX_ENTRIES = 100_000
CSV_FIELDS = ["path", "status", "size_bytes", "extension", "magic_hex", "error"]


def _open_directory(root_fd: int, parts: tuple[str, ...]) -> int:
    """Reopen each component relative to the pinned root, refusing symlinks."""
    fd = os.dup(root_fd)
    try:
        for part in parts:
            child = os.open(part, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=fd)
            os.close(fd)
            fd = child
        return fd
    except OSError:
        os.close(fd)
        raise


def inventory(root: Path | str) -> dict:
    """Scan a directory without following symlinks or opening special files."""
    root = Path(root)
    if root.is_symlink() or not root.is_dir():
        raise ValueError("root must be an existing directory, not a symlink")
    root = root.resolve()
    files, skipped, errors = [], [], []
    pending = [root]
    count = 0
    root_fd = os.open(root, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        while pending:
            directory = pending.pop()
            relative_path = directory.relative_to(root)
            relative = relative_path.as_posix()
            try:
                directory_fd = _open_directory(root_fd, relative_path.parts)
                try:
                    with os.scandir(directory_fd) as entries:
                        for entry in entries:
                            count += 1
                            if count > MAX_ENTRIES:
                                raise ValueError(f"inventory exceeds the {MAX_ENTRIES}-entry limit; scan a smaller root")
                            path = directory / entry.name
                            name = path.relative_to(root).as_posix()
                            try:
                                info = entry.stat(follow_symlinks=False)
                                if stat.S_ISLNK(info.st_mode):
                                    skipped.append({"path": name, "reason": "symlink"})
                                elif stat.S_ISDIR(info.st_mode):
                                    pending.append(path)
                                elif stat.S_ISREG(info.st_mode):
                                    fd = os.open(entry.name, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK, dir_fd=directory_fd)
                                    with os.fdopen(fd, "rb") as stream:
                                        current = os.fstat(stream.fileno())
                                        if not stat.S_ISREG(current.st_mode):
                                            raise OSError("entry changed to a non-regular file")
                                        magic = stream.read(MAGIC_BYTES)
                                    files.append({"path": name, "size_bytes": current.st_size,
                                                  "extension": path.suffix.lower(), "magic_hex": magic.hex()})
                                else:
                                    skipped.append({"path": name, "reason": "special file"})
                            except OSError as exc:
                                errors.append({"path": name, "error": str(exc)})
                finally:
                    os.close(directory_fd)
            except OSError as exc:
                errors.append({"path": relative, "error": str(exc)})
    finally:
        os.close(root_fd)
    files.sort(key=lambda item: item["path"])
    skipped.sort(key=lambda item: item["path"])
    errors.sort(key=lambda item: item["path"])
    return {"schema_version": 1, "root_name": root.name, "magic_bytes": MAGIC_BYTES,
            "file_count": len(files), "total_size_bytes": sum(item["size_bytes"] for item in files),
            "complete": not errors, "files": files, "skipped": skipped, "errors": errors}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--json", type=Path, help="new JSON report path")
    parser.add_argument("--csv", type=Path, help="new CSV report path")
    args = parser.parse_args(argv)
    try:
        destinations = [p for p in (args.json, args.csv) if p is not None]
        if len({p.resolve() for p in destinations}) != len(destinations):
            raise ValueError("JSON and CSV destinations must differ")
        for path in destinations:
            if path.exists() or path.is_symlink():
                raise ValueError(f"report already exists: {path}")
        report = inventory(args.root)
        output = json.dumps(report, indent=2, ensure_ascii=True) + "\n"
        if args.json:
            with args.json.open("x", encoding="utf-8") as stream:
                stream.write(output)
        if args.csv:
            with args.csv.open("x", encoding="utf-8", newline="") as stream:
                writer = csv.DictWriter(stream, fieldnames=CSV_FIELDS)
                writer.writeheader()
                for item in report["files"]:
                    writer.writerow({**item, "status": "ok", "error": ""})
                for item in report["skipped"]:
                    writer.writerow({"path": item["path"], "status": "skipped", "error": item["reason"]})
                for item in report["errors"]:
                    writer.writerow({**item, "status": "error"})
        if not destinations:
            print(output, end="")
        if report["errors"]:
            print(f"asset_inventory: incomplete scan ({len(report['errors'])} errors); see report", file=sys.stderr)
            return 1
        return 0
    except (OSError, ValueError) as exc:
        print(f"asset_inventory: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
