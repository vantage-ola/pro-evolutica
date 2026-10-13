"""Map static PE32 import/ASCII anchors; byte matches are not decoded code references."""

from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
import math
import os
from pathlib import Path
import stat
import struct
import sys

import pefile

if __package__:
    from .pe_inspect import inspect_pe_bytes
else:
    from pe_inspect import inspect_pe_bytes

MAX_BYTES = 64 * 1024 * 1024
MAX_SELECTORS = 16
MAX_MATCHES = 128
MAX_SCAN_BYTES = 1024 * 1024 * 1024
DEFAULT_IMPORTS = (
    'CreateFileA', 'CreateFileW', 'ReadFile', 'SetFilePointer',
    'Direct3DCreate9', 'QueryPerformanceCounter', 'PeekMessageA',
)


def matches(data, pattern):
    result = []
    start = 0
    while (offset := data.find(pattern, start)) != -1:
        if len(result) == MAX_MATCHES:
            raise ValueError('anchor occurrence limit exceeded; choose a narrower selector')
        result.append(offset)
        start = offset + 1
    return result


def selectors(values):
    if len(values) > MAX_SELECTORS or len(set(values)) != len(values):
        raise ValueError('selectors must be distinct and at most 16 per kind')
    for value in values:
        if not 1 <= len(value) <= 256 or any(not 32 <= ord(c) <= 126 for c in value):
            raise ValueError('selectors must be printable ASCII strings of 1–256 bytes')


def inspect_recon(path, *, strings=(), imports=DEFAULT_IMPORTS):
    selectors(strings)
    selectors(imports)
    path = Path(path)
    fd = os.open(path, os.O_RDONLY | os.O_NONBLOCK | os.O_NOFOLLOW)
    with os.fdopen(fd, 'rb') as stream:
        before = os.fstat(stream.fileno())
        if not stat.S_ISREG(before.st_mode) or before.st_size > MAX_BYTES:
            raise ValueError('recon input must be a regular file of at most 64 MiB')
        data = stream.read(MAX_BYTES + 1)
        after = os.fstat(stream.fileno())
    if (len(data) != before.st_size or len(data) > MAX_BYTES or
        (before.st_size, before.st_mtime_ns, before.st_ctime_ns) !=
        (after.st_size, after.st_mtime_ns, after.st_ctime_ns)):
        raise ValueError('recon input changed during read')
    metadata = inspect_pe_bytes(data, file_name=path.name)
    if metadata['architecture'] != 'x86' or metadata['pe_format'] != 'PE32':
        raise ValueError('static anchor patterns currently support x86 PE32 only')
    with pefile.PE(data=data, fast_load=True) as pe:
        pe.parse_data_directories(directories=[1, 13])
        base = pe.OPTIONAL_HEADER.ImageBase
        sections = [(s, s.get_data()) for s in pe.sections]

        def location(section, offset):
            if base + section.VirtualAddress + offset > 0xffffffff:
                raise ValueError('mapped address exceeds PE32 range')
            return {
                'section': section.Name.rstrip(b'\0').decode('ascii', errors='backslashreplace'),
                'file_offset': section.PointerToRawData + offset,
                'rva': section.VirtualAddress + offset,
                'va': base + section.VirtualAddress + offset,
            }

        scanned = 0

        def references(pattern, executable_only=False):
            nonlocal scanned
            result = []
            for section, chunk in sections:
                if executable_only and not section.Characteristics & 0x20000000:
                    continue
                scanned += len(chunk)
                if scanned > MAX_SCAN_BYTES:
                    raise ValueError('byte-scan work limit exceeded; use fewer selectors')
                for offset in matches(chunk, pattern):
                    result.append(location(section, offset))
                    if len(result) > MAX_MATCHES:
                        raise ValueError('reference occurrence limit exceeded')
            return result

        mapped_imports = []
        for attribute, kind in (('DIRECTORY_ENTRY_IMPORT', 'standard'), ('DIRECTORY_ENTRY_DELAY_IMPORT', 'delay')):
            for dll in getattr(pe, attribute, []):
                for symbol in dll.imports:
                    if symbol.name is None or symbol.name.decode('ascii', errors='backslashreplace') not in imports:
                        continue
                    address = symbol.address
                    if not 0 <= address <= 0xffffffff:
                        raise ValueError('import address exceeds PE32 range')
                    if len(mapped_imports) >= 64:
                        raise ValueError('selected import limit exceeded')
                    mapped_imports.append({
                        'dll': dll.dll.decode('ascii', errors='backslashreplace'),
                        'name': symbol.name.decode('ascii'), 'kind': kind, 'iat_va': address,
                        'call_byte_candidates': references(b'\xff\x15' + struct.pack('<I', address), True),
                        'jump_byte_candidates': references(b'\xff\x25' + struct.pack('<I', address), True),
                    })
        anchors = []
        for value in strings:
            locations = references(value.encode('ascii') + b'\0')
            for item in locations:
                item['pointer_byte_candidates'] = references(struct.pack('<I', item['va']))
            anchors.append({'ascii': value, 'occurrences': locations})
        section_report = []
        for section, chunk in sections:
            n = len(chunk)
            counts = Counter(chunk)
            section_report.append({
                **location(section, 0), 'raw_size': n, 'virtual_size': section.Misc_VirtualSize,
                'executable': bool(section.Characteristics & 0x20000000),
                'writable': bool(section.Characteristics & 0x80000000),
                'entropy_bits_per_byte': -sum(v/n * math.log2(v/n) for v in counts.values()) if n else None,
                'sha256': hashlib.sha256(chunk).hexdigest(),
            })
        debug = pe.OPTIONAL_HEADER.DATA_DIRECTORY[6] if len(pe.OPTIONAL_HEADER.DATA_DIRECTORY)>6 else None
        return {
            'schema_version': 1, 'file_name': path.name, 'sha256': metadata['sha256'],
            'file_size': len(data), 'architecture': metadata['architecture'],
            'image_base': base, 'entry_point': metadata['entry_point'],
            'sections': section_report, 'imports': mapped_imports, 'strings': anchors,
            'warnings': metadata['warnings'], 'scanned_bytes': scanned,
            'debug_directory': {
                'rva': debug.VirtualAddress, 'size': debug.Size,
                'multiple_of_28': debug.Size % 28 == 0,
            } if debug else None,
            'limitations': 'Raw byte candidates, not instruction-decoded xrefs or function boundaries. '
                           'Entropy is descriptive and does not prove packing/encryption. Missing patterns do not prove an API is unused. '
                           'No execution, unpacking, decryption or binary modification.',
        }


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', required=True, type=Path)
    parser.add_argument('--string', action='append', default=[], dest='strings', help='NUL-terminated ASCII byte anchor, repeatable')
    parser.add_argument('--import', action='append', dest='imports', help='import name, repeatable; default file/render/timing APIs')
    parser.add_argument('--json', type=Path, help='new report; default stdout')
    args = parser.parse_args(argv)
    try:
        if args.json and (args.json.exists() or args.json.is_symlink()):
            raise ValueError('output already exists')
        report = inspect_recon(args.input, strings=args.strings, imports=DEFAULT_IMPORTS if args.imports is None else args.imports)
        output = json.dumps(report, indent=2, allow_nan=False) + '\n'
        if args.json:
            with args.json.open('x', encoding='utf-8') as stream:
                stream.write(output)
        else:
            print(output, end='')
        return 0
    except (OSError, ValueError, pefile.PEFormatError) as error:
        print(f'pe_recon: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
