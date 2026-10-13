"""Inspect Windows PE headers and static imports without executing the input."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import stat
import struct
import sys

import pefile

MAX_FILE_BYTES = 256 * 1024 * 1024
MAX_SECTIONS = 96
MAX_IMPORT_DLLS = 1024
MACHINES = {0x14C: "x86", 0x8664: "x86_64", 0x1C4: "arm", 0xAA64: "arm64"}


def _text(value: bytes) -> str:
    return value.rstrip(b"\0").decode("ascii", errors="backslashreplace")


def inspect_pe(path: Path | str) -> dict:
    """Return metadata from one bounded, immutable in-memory file snapshot."""
    path = Path(path)
    # O_NONBLOCK prevents a FIFO from blocking before the regular-file check.
    fd = os.open(path, os.O_RDONLY | os.O_NONBLOCK)
    with os.fdopen(fd, "rb") as stream:
        info = os.fstat(stream.fileno())
        if not stat.S_ISREG(info.st_mode):
            raise ValueError("input must be a regular file")
        if info.st_size > MAX_FILE_BYTES:
            raise ValueError(f"input exceeds the {MAX_FILE_BYTES}-byte inspection limit")
        data = stream.read(MAX_FILE_BYTES + 1)
    if len(data) > MAX_FILE_BYTES:
        raise ValueError("input grew beyond the inspection limit")
    if len(data) < 64 or data[:2] != b"MZ":
        raise ValueError("not a Windows PE file: missing DOS header")
    pe_offset = struct.unpack_from("<I", data, 60)[0]
    if pe_offset < 64 or pe_offset + 24 > len(data):
        raise ValueError("invalid or truncated PE header offset")
    if data[pe_offset:pe_offset + 4] != b"PE\0\0":
        raise ValueError("not a Windows PE file: missing PE signature")
    section_count = struct.unpack_from("<H", data, pe_offset + 6)[0]
    optional_size = struct.unpack_from("<H", data, pe_offset + 20)[0]
    optional_offset = pe_offset + 24
    section_offset = optional_offset + optional_size
    if not 1 <= section_count <= MAX_SECTIONS:
        raise ValueError(f"section count must be between 1 and {MAX_SECTIONS}")
    if optional_size < 2 or section_offset + section_count * 40 > len(data):
        raise ValueError("truncated optional header or section table")
    magic = struct.unpack_from("<H", data, optional_offset)[0]
    minimum_size = {0x10B: 96, 0x20B: 112}.get(magic)
    if minimum_size is None or optional_size < minimum_size:
        raise ValueError("unsupported or truncated PE optional header")
    directory_count = struct.unpack_from("<I", data, optional_offset + minimum_size - 4)[0]
    if directory_count > 16 or minimum_size + directory_count * 8 > optional_size:
        raise ValueError("invalid or truncated data directory table")
    for i in range(section_count):
        raw_size, raw_offset = struct.unpack_from("<II", data, section_offset + i * 40 + 16)
        if raw_size and (raw_offset < section_offset + section_count * 40 or raw_offset + raw_size > len(data)):
            raise ValueError("section raw data lies outside the file or overlaps headers")

    try:
        pe = pefile.PE(data=data, fast_load=True)
        with pe:
            # Check the descriptor list before letting pefile parse its symbols.
            # The parser itself caps the total number of import symbols at 8192.
            for index, width in ((1, 20), (13, 32)):
                if index >= len(pe.OPTIONAL_HEADER.DATA_DIRECTORY):
                    continue
                directory = pe.OPTIONAL_HEADER.DATA_DIRECTORY[index]
                if not directory.VirtualAddress and not directory.Size:
                    continue
                if not directory.VirtualAddress or not width <= directory.Size <= len(data):
                    raise ValueError("invalid import directory")
                for i in range(MAX_IMPORT_DLLS + 1):
                    if (i + 1) * width > directory.Size:
                        raise ValueError("import descriptor table lacks a terminator within its declared size")
                    descriptor = pe.get_data(directory.VirtualAddress + i * width, width)
                    if len(descriptor) != width:
                        raise ValueError("truncated import descriptor table")
                    if descriptor == bytes(width):
                        break
                else:
                    raise ValueError("import descriptor table exceeds limit or lacks terminator")
            pe.parse_data_directories(directories=[1, 13])
            imports = []
            for attribute, kind in (("DIRECTORY_ENTRY_IMPORT", "standard"), ("DIRECTORY_ENTRY_DELAY_IMPORT", "delay")):
                for dll in getattr(pe, attribute, []):
                    imports.append({
                        "dll": _text(dll.dll),
                        "kind": kind,
                        "functions": [
                            {"name": _text(symbol.name) if symbol.name is not None else None,
                             "ordinal": symbol.ordinal if symbol.import_by_ordinal else None}
                            for symbol in dll.imports
                        ],
                    })
            entry_rva = pe.OPTIONAL_HEADER.AddressOfEntryPoint
            return {
                "schema_version": 1,
                "file_name": path.name,
                "sha256": hashlib.sha256(data).hexdigest(),
                "file_size": len(data),
                "architecture": MACHINES.get(pe.FILE_HEADER.Machine, "unknown"),
                "machine": f"0x{pe.FILE_HEADER.Machine:04x}",
                "bits": 32 if magic == 0x10B else 64,
                "pe_format": "PE32" if magic == 0x10B else "PE32+",
                "entry_point": {"rva": entry_rva, "va": pe.OPTIONAL_HEADER.ImageBase + entry_rva},
                "image_base": pe.OPTIONAL_HEADER.ImageBase,
                "sections": [{"name": _text(s.Name), "virtual_address": s.VirtualAddress,
                              "virtual_size": s.Misc_VirtualSize, "raw_offset": s.PointerToRawData,
                              "raw_size": s.SizeOfRawData} for s in pe.sections],
                "imports": imports,
                "warnings": pe.get_warnings(),
            }
    except pefile.PEFormatError as exc:
        raise ValueError(f"malformed PE file: {exc}") from exc


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path, help="local Windows PE file")
    parser.add_argument("--json", type=Path, help="new report path (existing files are refused)")
    args = parser.parse_args(argv)
    try:
        report = inspect_pe(args.input)
        output = json.dumps(report, indent=2, ensure_ascii=True) + "\n"
        if args.json:
            with args.json.open("x", encoding="utf-8") as stream:
                stream.write(output)
        else:
            print(output, end="")
        return 0
    except (OSError, ValueError) as exc:
        print(f"pe_inspect: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
