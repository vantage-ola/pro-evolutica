"""Original, synthetic PE bytes generated in memory; no commercial fixtures."""

import struct

import pytest


def make_pe(*, bits=32, machine=None, imports=True, delay=False):
    data = bytearray(0x800)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 60, 0x80)
    data[0x80:0x84] = b"PE\0\0"
    optional_size = 224 if bits == 32 else 240
    machine = machine if machine is not None else (0x14C if bits == 32 else 0x8664)
    struct.pack_into("<HHIIIHH", data, 0x84, machine, 2, 0, 0, 0, optional_size, 0x102 if bits == 32 else 0x22)
    optional = 0x98
    struct.pack_into("<H", data, optional, 0x10B if bits == 32 else 0x20B)
    struct.pack_into("<III", data, optional + 4, 0x200, 0x400, 0)
    struct.pack_into("<II", data, optional + 16, 0x1000, 0x1000)
    if bits == 32:
        struct.pack_into("<II", data, optional + 24, 0x2000, 0x400000)
        directory_offset = optional + 96
    else:
        struct.pack_into("<Q", data, optional + 24, 0x140000000)
        directory_offset = optional + 112
    struct.pack_into("<II", data, optional + 32, 0x1000, 0x200)
    struct.pack_into("<HH", data, optional + 40, 6, 0)
    struct.pack_into("<HH", data, optional + 48, 6, 0)
    struct.pack_into("<II", data, optional + 56, 0x3000, 0x200)
    struct.pack_into("<H", data, optional + 68, 3)
    struct.pack_into("<I", data, directory_offset - 4, 16)
    section_offset = optional + optional_size
    for i, (name, rva, raw_size, raw_offset, flags) in enumerate([
        (b".text", 0x1000, 0x200, 0x200, 0x60000020),
        (b".idata", 0x2000, 0x400, 0x400, 0xC0000040),
    ]):
        struct.pack_into("<8sIIIIIIHHI", data, section_offset + i * 40,
                         name, raw_size, rva, raw_size, raw_offset, 0, 0, 0, 0, flags)
    data[0x200] = 0xC3  # RET; never executed by tests or tools.
    if imports:
        struct.pack_into("<II", data, directory_offset + 8, 0x2000, 40)
        struct.pack_into("<IIIII", data, 0x400, 0x2050, 0, 0, 0x2090, 0x2070)
        thunk_format = "<III" if bits == 32 else "<QQQ"
        ordinal = (1 << (bits - 1)) | 7
        struct.pack_into(thunk_format, data, 0x450, 0x20A0, ordinal, 0)
        struct.pack_into(thunk_format, data, 0x470, 0x20A0, ordinal, 0)
        data[0x490:0x49D] = b"KERNEL32.dll\0"
        data[0x4A0:0x4AE] = b"\0\0ExitProcess\0"
    if delay:
        struct.pack_into("<II", data, directory_offset + 13 * 8, 0x2100, 64)
        struct.pack_into("<IIIIIIII", data, 0x500, 1, 0x2190, 0x21C0, 0x2170, 0x2150, 0, 0, 0)
        thunk_format = "<II" if bits == 32 else "<QQ"
        struct.pack_into(thunk_format, data, 0x550, 0x21A0, 0)
        struct.pack_into(thunk_format, data, 0x570, 0x21A0, 0)
        data[0x590:0x59B] = b"USER32.dll\0"
        data[0x5A0:0x5AE] = b"\0\0MessageBoxA\0"
    return bytes(data)


@pytest.fixture
def pe_factory(tmp_path):
    def create(**kwargs):
        path = tmp_path / "synthetic.exe"
        path.write_bytes(make_pe(**kwargs))
        return path
    return create
