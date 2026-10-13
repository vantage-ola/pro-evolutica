import binascii
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import zlib

import pytest

from tools import pes_texture


def dds(width=4, height=4, mips=1, blocks=None):
    header = [0] * 31
    header[0:7] = [124, 0xA1007, height, width, 8, 0, mips]
    header[18:21] = [32, 4, int.from_bytes(b"DXT1", "little")]
    header[26] = 0x1000
    sizes = [max(1, (max(1, width >> i) + 3) // 4) * max(1, (max(1, height >> i) + 3) // 4) * 8
             for i in range(mips)]
    payload = blocks if blocks is not None else struct.pack("<HHI", 0xF800, 0, 0) * (sum(sizes) // 8)
    return b"DDS " + struct.pack("<31I", *header) + payload


def bundle(chunks):
    data = bytearray(struct.pack("<II", len(chunks), 8) + bytes(len(chunks) * 12))
    data.extend(bytes((-len(data)) % 16))
    for index, chunk in enumerate(chunks):
        struct.pack_into("<III", data, 8 + index * 12, len(data), len(chunk), 0xFFFFFFF0)
        data.extend(chunk)
    return bytes(data)


def texture(dds_bytes, padding=0):
    return b"WE00" + struct.pack("<III", 0xFF000123, len(dds_bytes), 0x100000) + dds_bytes + bytes(padding)


def set_u32(data, offset, value):
    data = bytearray(data)
    struct.pack_into("<I", data, offset, value)
    return bytes(data)


def test_bundle_selection_and_byte_exact_dds():
    image = dds(8, 8, 4)
    data = bundle([b"non-texture metadata", texture(image)])
    extracted, report = pes_texture.extract_texture(data, 1)
    assert extracted == image
    assert report["input_sha256"] == hashlib.sha256(data).hexdigest()
    assert report["entry_count"] == 2
    assert report["entry"]["index"] == 1
    assert report["entry"]["unknown_u32"] == 0xFFFFFFF0
    assert data[report["dds_offset"]:report["dds_offset"] + len(image)] == image
    assert report["dds"]["width"] == report["dds"]["height"] == 8
    assert report["dds"]["mip_count"] == 4
    assert [x["size_bytes"] for x in report["dds"]["levels"]] == [32, 8, 8, 8]
    assert report["dds"]["sha256"] == hashlib.sha256(image).hexdigest()


def test_dxt1_opaque_golden_colors_and_selector_order():
    # Each row selects red, blue, 2/3 red, 2/3 blue from left to right.
    block = struct.pack("<HHI", 0xF800, 0x001F, 0xE4E4E4E4)
    width, height, pixels = pes_texture.decode_dxt1(dds(blocks=block))
    expected_row = bytes([255, 0, 0, 255, 0, 0, 255, 255, 170, 0, 85, 255, 85, 0, 170, 255])
    assert (width, height) == (4, 4)
    assert pixels == expected_row * 4


def test_dxt1_transparent_golden_colors():
    block = struct.pack("<HHI", 0x001F, 0xF800, 0xE4E4E4E4)
    _, _, pixels = pes_texture.decode_dxt1(dds(blocks=block))
    expected_row = bytes([0, 0, 255, 255, 255, 0, 0, 255, 127, 0, 127, 255, 0, 0, 0, 0])
    assert pixels == expected_row * 4
    _, _, equal = pes_texture.decode_dxt1(dds(blocks=struct.pack("<HHI", 0, 0, 0xFFFFFFFF)))
    assert equal == bytes(4 * 4 * 4)


def test_non_multiple_of_four_clips_blocks_and_small_mip():
    blocks = struct.pack("<HHI", 0xF800, 0, 0) + struct.pack("<HHI", 0x07E0, 0, 0)
    width, height, pixels = pes_texture.decode_dxt1(dds(5, 3, blocks=blocks))
    assert (width, height) == (5, 3)
    assert pixels == (bytes([255, 0, 0, 255]) * 4 + bytes([0, 255, 0, 255])) * 3
    _, _, one = pes_texture.decode_dxt1(dds(1, 1))
    assert one == bytes([255, 0, 0, 255])


def test_png_dimensions_crc_and_decoded_scanlines():
    png = pes_texture.dxt1_png(dds())
    assert png[:8] == b"\x89PNG\r\n\x1a\n"
    offset, kinds, compressed = 8, [], bytearray()
    while offset < len(png):
        size = struct.unpack_from(">I", png, offset)[0]
        kind, content = png[offset + 4:offset + 8], png[offset + 8:offset + 8 + size]
        assert binascii.crc32(kind + content) == struct.unpack_from(">I", png, offset + 8 + size)[0]
        kinds.append(kind)
        if kind == b"IHDR":
            assert struct.unpack(">IIBBBBB", content) == (4, 4, 8, 6, 0, 0, 0)
        elif kind == b"IDAT":
            compressed.extend(content)
        offset += size + 12
    assert kinds == [b"IHDR", b"IDAT", b"IEND"]
    assert zlib.decompress(compressed) == (b"\0" + bytes([255, 0, 0, 255]) * 4) * 4


@pytest.mark.parametrize("data, message", [
    (b"", "bundle header"),
    (struct.pack("<II", 0, 8), "count"),
    (struct.pack("<II", 4097, 8), "count"),
    (struct.pack("<II", 1, 12), "table offset"),
    (struct.pack("<II", 1, 8), "bundle table"),
    (set_u32(bundle([texture(dds())]), 8, 8), "overlaps"),
    (set_u32(bundle([texture(dds())]), 12, 0xFFFFFFFF), "out of bounds"),
    (bundle([b"metadata"]), "not a supported WE00"),
    (bundle([set_u32(texture(dds()), 8, 0xFFFFFFFF)]), "DDS length"),
    (bundle([texture(dds(), padding=16)]), "trailing data"),
    (bundle([texture(dds()) + b"X"]), "trailing data"),
])
def test_bundle_rejections(data, message):
    with pytest.raises(ValueError, match=message):
        pes_texture.extract_texture(data)


@pytest.mark.parametrize("data, message", [
    (b"DDS ", "truncated"),
    (b"nope" + dds()[4:], "DDS header"),
    (set_u32(dds(), 4, 123), "header.*size"),
    (set_u32(dds(), 76, 31), "pixel-format size"),
    (set_u32(dds(), 16, 0), "dimensions"),
    (set_u32(dds(), 12, 4097), "dimensions"),
    (set_u32(set_u32(dds(), 12, 4096), 16, 4096), "dimensions"),
    (set_u32(dds(), 112, 0x200), "flat 2D"),
    (set_u32(dds(), 24, 2), "flat 2D"),
    (set_u32(dds(), 84, int.from_bytes(b"DXT5", "little")), "only DXT1"),
    (set_u32(dds(), 80, 0), "only DXT1"),
    (set_u32(dds(), 28, 4), "mip count"),
    (dds()[:-1], "length mismatch"),
    (dds() + b"X", "length mismatch"),
])
def test_dds_rejections(data, message):
    with pytest.raises(ValueError, match=message):
        pes_texture.inspect_dds(data)


def test_zero_mip_count_and_we00_padding():
    data = set_u32(dds(), 28, 0)
    image, metadata = pes_texture.extract_texture(bundle([texture(data, padding=15)]))
    assert image == data
    assert metadata["dds"]["mip_count"] == 1


@pytest.mark.parametrize("index", [-1, 1])
def test_bad_bundle_index(index):
    with pytest.raises(ValueError, match="texture index"):
        pes_texture.extract_texture(bundle([texture(dds())]), index)


def test_invalid_unselected_entry_is_rejected():
    data = set_u32(bundle([texture(dds()), b"other"]), 20, 0xFFFFFFFF)
    with pytest.raises(ValueError, match="bundle entry 1"):
        pes_texture.extract_texture(data)


def test_input_limit(tmp_path, monkeypatch):
    data = bundle([texture(dds())])
    path = tmp_path / "input.bin"
    path.write_bytes(data)
    monkeypatch.setattr(pes_texture, "MAX_INPUT_SIZE", len(data) - 1)
    with pytest.raises(ValueError, match="input exceeds"):
        pes_texture.extract_texture(data)
    with pytest.raises(ValueError, match="input exceeds"):
        pes_texture.inspect_texture(path)


def test_cli_extraction_preview_and_metadata(tmp_path):
    path = tmp_path / "input.bin"
    original = bundle([texture(dds())])
    path.write_bytes(original)
    image, png, report = tmp_path / "texture.dds", tmp_path / "texture.png", tmp_path / "report.json"
    result = subprocess.run([sys.executable, str(Path(pes_texture.__file__)), "--input", str(path),
                             "--output", str(image), "--png", str(png), "--json", str(report)],
                            cwd=tmp_path, capture_output=True, text=True, timeout=5)
    assert result.returncode == 0 and result.stdout == ""
    assert image.read_bytes() == dds()
    metadata = json.loads(report.read_text())
    assert metadata["png"]["sha256"] == hashlib.sha256(png.read_bytes()).hexdigest()
    assert metadata["dds_output_name"] == "texture.dds"
    assert path.read_bytes() == original


def test_inspect_only_stdout(tmp_path, capsys):
    path = tmp_path / "input.bin"
    path.write_bytes(bundle([texture(dds())]))
    assert pes_texture.main(["--input", str(path)]) == 0
    assert json.loads(capsys.readouterr().out)["dds"]["pixel_format"] == "DXT1"
    assert list(tmp_path.iterdir()) == [path]


def test_errors_create_no_outputs_and_destinations_preflight(tmp_path):
    path, output, png, report = (tmp_path / name for name in ["input.bin", "out.dds", "out.png", "out.json"])
    path.write_bytes(bundle([b"not a texture"]))
    args = ["--input", str(path), "--output", str(output), "--png", str(png), "--json", str(report)]
    assert pes_texture.main(args) == 1
    assert not output.exists() and not png.exists() and not report.exists()
    path.write_bytes(bundle([texture(dds())]))
    report.write_text("keep")
    assert pes_texture.main(args) == 1
    assert not output.exists() and not png.exists() and report.read_text() == "keep"
    assert pes_texture.main(["--input", str(path), "--output", str(output), "--png", str(output)]) == 1
    assert not output.exists()
    assert pes_texture.main(["--input", str(path), "--output", str(tmp_path / "absent" / "out")]) == 1


@pytest.mark.parametrize("kind", ["file", "symlink", "broken_symlink", "source"])
def test_refuses_output_overwrites(tmp_path, kind):
    path, output = tmp_path / "input.bin", tmp_path / "out.dds"
    data = bundle([texture(dds())])
    path.write_bytes(data)
    if kind == "file":
        output.write_bytes(b"keep")
    elif kind == "source":
        output = path
    else:
        output.symlink_to(path if kind == "symlink" else tmp_path / "missing")
    with pytest.raises(ValueError, match="already exists"):
        pes_texture.inspect_texture(path, output=output)
    assert path.read_bytes() == data
    if kind == "file":
        assert output.read_bytes() == b"keep"


@pytest.mark.parametrize("kind", ["missing", "directory", "symlink", "fifo"])
def test_input_cli_errors_do_not_block(tmp_path, kind):
    path = tmp_path / kind
    if kind == "directory":
        path.mkdir()
    elif kind == "symlink":
        actual = tmp_path / "actual.bin"
        actual.write_bytes(bundle([texture(dds())]))
        path.symlink_to(actual)
    elif kind == "fifo":
        os.mkfifo(path)
    result = subprocess.run([sys.executable, str(Path(pes_texture.__file__)), "--input", str(path)],
                            capture_output=True, text=True, timeout=5)
    assert result.returncode == 1 and "Traceback" not in result.stderr
