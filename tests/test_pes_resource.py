"""Synthetic archives only; local commercial samples are checked separately."""

import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import zlib

import pytest

from tools import afs_inspect, pes_resource, wesys_unpack


def make_archive(tmp_path, *, selector=0x0b, stored=False):
    payload = b"original synthetic resource" * 20
    body = payload if stored else zlib.compress(payload)
    raw = (b"\0\x01" + bytes([0 if stored else 1]) + b"WESYS" +
           struct.pack("<II", len(body), 0 if stored else len(payload)) + body + bytes(3))
    # Empty slot 0, selected entry 1, unrelated entry 2 and sector padding.
    data = bytearray(4096)
    struct.pack_into("<4sI", data, 0, b"AFS\0", 3)
    struct.pack_into("<IIIIII", data, 8, 0, 0, 2048, len(raw), 4000, 4)
    data[2048:2048 + len(raw)] = raw
    data[4000:4004] = b"skip"
    archive = tmp_path / f"dt{selector:02x}.img"
    archive.write_bytes(data)
    return archive, bytes(data), raw, payload


@pytest.mark.parametrize("selector", [0x0b, 0x0c, 0x0d, 0x0e])
@pytest.mark.parametrize("stored", [False, True])
def test_id_selects_archive_entry_and_exact_payload(tmp_path, monkeypatch, selector, stored):
    archive, original, raw, payload = make_archive(tmp_path, selector=selector, stored=stored)
    reads = []
    original_read = afs_inspect._read_exact
    def record_read(stream, size):
        reads.append((stream.tell(), size))
        return original_read(stream, size)
    monkeypatch.setattr(afs_inspect, "_read_exact", record_read)
    decoded, report = pes_resource.load_resource(tmp_path, (selector << 24) | 1)
    assert decoded == payload
    assert reads == [(0, 8), (8, 24), (2048, len(raw))]
    assert report["route"] == "base_archive"
    assert report["resource_id_hex"] == f"0x{selector:02x}000001"
    assert report["archive"]["file_name"] == archive.name
    assert report["archive"]["entry"] == {"index": 1, "offset": 2048, "size_bytes": len(raw)}
    assert report["archive"]["entry_sha256"] == hashlib.sha256(raw).hexdigest()
    assert report["wesys"]["decoded_sha256"] == hashlib.sha256(payload).hexdigest()
    assert report["wesys"]["input_size_bytes"] == len(raw)
    assert archive.read_bytes() == original


@pytest.mark.parametrize("resource_id, message", [
    (-1, "unsigned 32-bit"), (0x100000000, "unsigned 32-bit"),
    (True, "unsigned 32-bit"), (0x0a000001, "unsupported archive"),
    (0x0b000000, "empty slot"), (0x0bffffff, "outside.*archive"),
])
def test_invalid_ids_and_slots(tmp_path, resource_id, message):
    make_archive(tmp_path)
    with pytest.raises(ValueError, match=message):
        pes_resource.load_resource(tmp_path, resource_id)


def test_entry_cap_checked_before_read(tmp_path, monkeypatch):
    make_archive(tmp_path)
    monkeypatch.setattr(wesys_unpack, "MAX_INPUT_SIZE", 16)
    original_read = afs_inspect._read_exact
    def index_reads_only(stream, size):
        assert stream.tell() < 32
        return original_read(stream, size)
    monkeypatch.setattr(afs_inspect, "_read_exact", index_reads_only)
    with pytest.raises(ValueError, match="read limit"):
        pes_resource.load_resource(tmp_path, 0x0b000001)


def test_validates_entire_index_before_reading_entry(tmp_path):
    archive, original, _, _ = make_archive(tmp_path)
    broken = bytearray(original)
    struct.pack_into("<II", broken, 24, len(broken), 1)  # Unselected entry beyond EOF.
    archive.write_bytes(broken)
    with pytest.raises(ValueError, match="entry 2 extends beyond"):
        pes_resource.load_resource(tmp_path, 0x0b000001)


def test_detects_archive_mutation(tmp_path, monkeypatch):
    archive, _, _, _ = make_archive(tmp_path)
    original_read = afs_inspect._read_exact
    def change_after_entry_read(stream, size):
        data = original_read(stream, size)
        if stream.tell() > 2048:
            with archive.open("ab") as changed:
                changed.write(b"changed")
        return data
    monkeypatch.setattr(afs_inspect, "_read_exact", change_after_entry_read)
    with pytest.raises(ValueError, match="input changed"):
        pes_resource.load_resource(tmp_path, 0x0b000001)


def test_cli_from_other_directory_and_no_overwrite(tmp_path):
    archive, original, _, payload = make_archive(tmp_path, selector=0x0e, stored=True)
    output, report = tmp_path / "decoded.bin", tmp_path / "report.json"
    args = ["--img-dir", str(tmp_path), "--id", "0x0e000001",
            "--output", str(output), "--json", str(report)]
    result = subprocess.run([sys.executable, str(Path(pes_resource.__file__)), *args],
                            cwd=tmp_path, capture_output=True, text=True, timeout=5)
    assert result.returncode == 0, result.stderr
    assert result.stdout == ""
    assert output.read_bytes() == payload
    assert json.loads(report.read_text())["output_name"] == output.name
    output.write_bytes(b"keep")
    assert pes_resource.main(args) == 1
    assert output.read_bytes() == b"keep"
    assert archive.read_bytes() == original


def test_failed_decode_and_destination_preflight_create_no_output(tmp_path):
    archive, original, raw, _ = make_archive(tmp_path)
    output, report = tmp_path / "decoded.bin", tmp_path / "report.json"
    args = ["--img-dir", str(tmp_path), "--id", "0x0b000001", "--output", str(output)]
    report.write_text("keep")
    assert pes_resource.main(args + ["--json", str(report)]) == 1
    assert report.read_text() == "keep" and not output.exists()
    assert pes_resource.main(args + ["--json", str(output)]) == 1
    assert not output.exists()
    report.unlink()
    broken = bytearray(original)
    broken[2048 + len(raw) - 1] = 1  # Invalid trailing padding.
    archive.write_bytes(broken)
    assert pes_resource.main(args + ["--json", str(report)]) == 1
    assert not output.exists() and not report.exists()
