import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys

import pytest

from tools import afs_inspect


def make_afs(payloads):
    """Original test bytes, with padded entries and empty slots."""
    data = bytearray(b"AFS\0" + struct.pack("<I", len(payloads)))
    data.extend(bytes(8 * len(payloads)))
    for index, payload in enumerate(payloads):
        data.extend(bytes((-len(data)) % 32))
        struct.pack_into("<II", data, 8 + index * 8, len(data), len(payload))
        data.extend(payload)
    return bytes(data)


def test_index_and_raw_extraction(tmp_path):
    payload = b"original fixture: not a game asset" * 3
    data = make_afs([b"", payload, b"\xff\0"])
    archive = tmp_path / "test.img"
    archive.write_bytes(data)
    output = tmp_path / "entry.bin"
    report = afs_inspect.inspect_afs(archive, entry=1, output=output)
    assert report["format"] == "AFS"
    assert report["file_name"] == archive.name
    assert report["file_size"] == len(data)
    assert report["sha256"] == hashlib.sha256(data).hexdigest()
    assert report["entry_count"] == 3
    assert report["empty_count"] == 1
    assert [item["index"] for item in report["entries"]] == [0, 1, 2]
    assert [item["magic_hex"] for item in report["entries"]] == ["", payload[:16].hex(), "ff00"]
    offset = report["entries"][1]["offset"]
    assert data[offset:offset + len(payload)] == output.read_bytes() == payload
    assert report["extraction"] == {
        "index": 1, "size_bytes": len(payload),
        "sha256": hashlib.sha256(payload).hexdigest(), "output_name": "entry.bin",
    }
    assert archive.read_bytes() == data


def test_empty_archive_and_zero_offset_slot(tmp_path):
    archive = tmp_path / "empty.afs"
    archive.write_bytes(make_afs([]))
    assert afs_inspect.inspect_afs(archive)["entries"] == []
    archive.write_bytes(b"AFS\0" + struct.pack("<III", 1, 0, 0))
    report = afs_inspect.inspect_afs(archive)
    assert report["empty_count"] == 1
    assert report["entries"][0]["magic_hex"] == ""


@pytest.mark.parametrize("data, message", [
    (b"AFS", "truncated"),
    (b"AFSX" + bytes(4), "not an AFS"),
    (b"AFS\0" + struct.pack("<I", 100_001), "entry limit"),
    (b"AFS\0" + struct.pack("<I", 1), "entry table"),
    (b"AFS\0" + struct.pack("<III", 1, 8, 1), "overlaps"),
    (b"AFS\0" + struct.pack("<III", 1, 16, 1), "beyond"),
    (b"AFS\0" + struct.pack("<III", 1, 0xffffffff, 2), "beyond"),
    (b"AFS\0" + struct.pack("<III", 1, 17, 0), "beyond"),
])
def test_malformed_archives(tmp_path, data, message):
    archive = tmp_path / "bad.img"
    archive.write_bytes(data)
    with pytest.raises(ValueError, match=message):
        afs_inspect.inspect_afs(archive)


@pytest.mark.parametrize("index", [-1, 3])
def test_bad_entry_index_produces_no_output(tmp_path, index):
    archive = tmp_path / "test.afs"
    archive.write_bytes(make_afs([b"abc"]))
    output = tmp_path / "entry.bin"
    with pytest.raises(ValueError, match="entry index"):
        afs_inspect.inspect_afs(archive, entry=index, output=output)
    assert not output.exists()


def test_empty_slot_and_extraction_limit(tmp_path, monkeypatch):
    archive = tmp_path / "test.afs"
    archive.write_bytes(make_afs([b"", b"abcd"]))
    output = tmp_path / "entry.bin"
    with pytest.raises(ValueError, match="empty slot"):
        afs_inspect.inspect_afs(archive, entry=0, output=output)
    monkeypatch.setattr(afs_inspect, "MAX_EXTRACT_SIZE", 3)
    with pytest.raises(ValueError, match="extraction limit"):
        afs_inspect.inspect_afs(archive, entry=1, output=output)
    assert not output.exists()


def test_streamed_hash_and_extraction(tmp_path, monkeypatch):
    monkeypatch.setattr(afs_inspect, "CHUNK_SIZE", 7)
    payload = bytes(range(255))
    archive = tmp_path / "test.afs"
    archive.write_bytes(make_afs([payload]))
    output = tmp_path / "entry.bin"
    report = afs_inspect.inspect_afs(archive, entry=0, output=output)
    assert report["sha256"] == hashlib.sha256(archive.read_bytes()).hexdigest()
    assert report["extraction"]["sha256"] == hashlib.sha256(payload).hexdigest()
    assert output.read_bytes() == payload


def test_file_size_limit_before_read(tmp_path, monkeypatch):
    archive = tmp_path / "test.afs"
    archive.write_bytes(make_afs([b"abc"]))
    monkeypatch.setattr(afs_inspect, "MAX_FILE_SIZE", 8)
    with pytest.raises(ValueError, match="byte limit"):
        afs_inspect.inspect_afs(archive)


@pytest.mark.parametrize("kind", ["missing", "directory", "symlink", "fifo"])
def test_invalid_input_cli_does_not_block(tmp_path, kind):
    archive = tmp_path / kind
    if kind == "directory":
        archive.mkdir()
    elif kind == "symlink":
        target = tmp_path / "actual.afs"
        target.write_bytes(make_afs([]))
        archive.symlink_to(target)
    elif kind == "fifo":
        os.mkfifo(archive)
    result = subprocess.run([sys.executable, str(Path(afs_inspect.__file__)),
                             "--input", str(archive)], capture_output=True, text=True, timeout=5)
    assert result.returncode == 1
    assert "Traceback" not in result.stderr
    assert result.stdout == ""


@pytest.mark.parametrize("kind", ["existing", "source", "symlink", "broken_symlink"])
def test_refuses_extraction_overwrites(tmp_path, kind):
    archive = tmp_path / "test.afs"
    original = make_afs([b"abc"])
    archive.write_bytes(original)
    output = tmp_path / "output"
    if kind == "existing":
        output.write_bytes(b"keep")
    elif kind == "source":
        output = archive
    elif kind == "symlink":
        output.symlink_to(archive)
    else:
        output.symlink_to(tmp_path / "missing")
    with pytest.raises(ValueError, match="already exists"):
        afs_inspect.inspect_afs(archive, entry=0, output=output)
    assert archive.read_bytes() == original
    if kind == "existing":
        assert output.read_bytes() == b"keep"


def test_cli_json_stdout_and_extraction(tmp_path, capsys):
    archive = tmp_path / "test.img"
    archive.write_bytes(make_afs([b"synthetic"]))
    assert afs_inspect.main(["--input", str(archive)]) == 0
    assert json.loads(capsys.readouterr().out)["entry_count"] == 1
    report, output = tmp_path / "report.json", tmp_path / "raw.bin"
    assert afs_inspect.main(["--input", str(archive), "--json", str(report),
                             "--entry", "0", "--output", str(output)]) == 0
    assert output.read_bytes() == b"synthetic"
    assert json.loads(report.read_text())["extraction"]["index"] == 0
    assert capsys.readouterr().out == ""


def test_cli_preflights_destinations(tmp_path):
    archive = tmp_path / "test.afs"
    archive.write_bytes(make_afs([b"abc"]))
    output, report = tmp_path / "raw.bin", tmp_path / "report.json"
    args = ["--input", str(archive), "--entry", "0", "--output", str(output)]
    report.write_text("keep")
    assert afs_inspect.main(args + ["--json", str(report)]) == 1
    assert report.read_text() == "keep"
    assert not output.exists()
    assert afs_inspect.main(args + ["--json", str(output)]) == 1
    assert not output.exists()
    assert afs_inspect.main(args + ["--json", str(tmp_path / "missing" / "report")]) == 1
    assert not output.exists()


@pytest.mark.parametrize("flag, value", [("--entry", "0"), ("--output", "raw.bin")])
def test_cli_requires_extraction_pair(tmp_path, flag, value):
    with pytest.raises(SystemExit) as exc:
        afs_inspect.main(["--input", str(tmp_path / "unused"), flag, value])
    assert exc.value.code == 2


def test_detects_input_mutation_before_extracting(tmp_path, monkeypatch):
    archive = tmp_path / "test.afs"
    data = make_afs([b"abc"])
    archive.write_bytes(data)
    output = tmp_path / "raw.bin"
    original = afs_inspect._read_exact
    def append_during_hash(stream, size):
        if stream.tell() == 0 and size == len(data):
            with archive.open("ab") as changed:
                changed.write(b"extra")
        return original(stream, size)
    monkeypatch.setattr(afs_inspect, "_read_exact", append_during_hash)
    with pytest.raises(ValueError, match="changed during inspection"):
        afs_inspect.inspect_afs(archive, entry=0, output=output)
    assert not output.exists()


def test_truncation_during_extraction_removes_partial_output(tmp_path, monkeypatch):
    archive = tmp_path / "test.afs"
    archive.write_bytes(make_afs([b"original payload"]))
    output = tmp_path / "raw.bin"
    original = afs_inspect._extract
    def truncate_before_copy(stream, item, destination):
        with archive.open("r+b") as changed:
            changed.truncate(item["offset"])
        # Drop bytes buffered during prior inspection reads.
        stream.seek(0, os.SEEK_END)
        return original(stream, item, destination)
    monkeypatch.setattr(afs_inspect, "_extract", truncate_before_copy)
    with pytest.raises(ValueError, match="truncated"):
        afs_inspect.inspect_afs(archive, entry=0, output=output)
    assert not output.exists()
