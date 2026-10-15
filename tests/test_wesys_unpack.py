import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tracemalloc
import zlib

import pytest

from tools import wesys_unpack


def wrapped(payload=b"original synthetic payload", *, padding=0):
    compressed = zlib.compress(payload)
    return b"\0\x01\x01WESYS" + struct.pack("<II", len(compressed), len(payload)) + compressed + bytes(padding)


def set_size(data, offset, value):
    data = bytearray(data)
    struct.pack_into("<I", data, offset, value)
    return bytes(data)


@pytest.mark.parametrize("payload, padding", [(b"", 0), (b"abc", 0), (bytes(range(256)), 3), (b"original" * 1000, 15)])
def test_decoding_lengths_hashes_and_padding(payload, padding):
    data = wrapped(payload, padding=padding)
    decoded, report = wesys_unpack.decode_wesys(data)
    assert decoded == payload
    assert report == {
        "schema_version": 1, "format": "WESYS", "version_hex": "000101",
        "compression": "zlib", "payload_size_bytes": len(data) - 16 - padding,
        "input_size_bytes": len(data), "input_sha256": hashlib.sha256(data).hexdigest(),
        "compressed_size_bytes": len(data) - 16 - padding, "padding_size_bytes": padding,
        "decoded_size_bytes": len(payload), "decoded_sha256": hashlib.sha256(payload).hexdigest(),
        "decoded_magic_hex": payload[:32].hex(),
    }


def stored(payload=b"synthetic stored payload", *, padding=0):
    return b"\0\x01\0WESYS" + struct.pack("<II", len(payload), 0) + payload + bytes(padding)


@pytest.mark.parametrize("payload, padding", [(b"", 0), (b"not a zlib stream", 15)])
def test_stored_payload_is_copied_exactly(payload, padding):
    decoded, report = wesys_unpack.decode_wesys(stored(payload, padding=padding))
    assert decoded == payload
    assert report["version_hex"] == "000100"
    assert report["compression"] == "none"
    assert report["compressed_size_bytes"] == 0
    assert report["payload_size_bytes"] == report["decoded_size_bytes"] == len(payload)
    assert report["decoded_sha256"] == hashlib.sha256(payload).hexdigest()
    assert report["padding_size_bytes"] == padding


@pytest.mark.parametrize("data, message", [
    (set_size(stored(), 12, 1), "stored.*header"),
    (set_size(stored(), 8, 0xffffffff), "payload size.*limit"),
    (stored()[:-1], "truncated.*payload"),
    (stored() + b"x", "trailing data"),
    (stored(padding=16), "trailing data"),
])
def test_rejects_malformed_stored_wrapper(data, message):
    with pytest.raises(ValueError, match=message):
        wesys_unpack.decode_wesys(data)


def test_stored_output_limit(monkeypatch):
    monkeypatch.setattr(wesys_unpack, "MAX_DECODED_SIZE", 2)
    with pytest.raises(ValueError, match="decoded size.*limit"):
        wesys_unpack.decode_wesys(stored(b"abc"))


@pytest.mark.parametrize("data, message", [
    (b"WESYS", "truncated.*header"),
    (b"\0\x01\x01OTHER" + bytes(8), "not a WESYS"),
    (b"\0\x10\x01" + wrapped()[3:], "unsupported.*version"),
    (set_size(wrapped(), 8, 0xffffffff), "compressed size.*limit"),
    (set_size(wrapped(), 12, 0xffffffff), "decoded size.*limit"),
    (set_size(wrapped(), 8, len(wrapped())), "truncated.*payload"),
    (wrapped() + b"\x01", "trailing data"),
    (wrapped(padding=16), "trailing data"),
    (set_size(wrapped(), 12, 1), "exceeds.*declared size"),
    (set_size(wrapped(), 12, 1000), "length differs"),
    (b"\0\x01\x01WESYS" + struct.pack("<II", 0, 0), "incomplete.*stream"),
])
def test_rejects_malformed_wrapper(data, message):
    with pytest.raises(ValueError, match=message):
        wesys_unpack.decode_wesys(data)


def test_zlib_checksum_and_truncated_stream():
    data = wrapped()
    corrupted = data[:-1] + bytes([data[-1] ^ 1])
    with pytest.raises(ValueError, match="invalid.*zlib"):
        wesys_unpack.decode_wesys(corrupted)
    shortened = set_size(data[:-2], 8, len(data) - 18)
    with pytest.raises(ValueError, match="incomplete.*stream"):
        wesys_unpack.decode_wesys(shortened)


@pytest.mark.parametrize("extra", [b"junk", zlib.compress(b"second stream")])
def test_rejects_data_hidden_inside_declared_compressed_range(extra):
    original = wrapped()
    data = set_size(original + extra, 8, len(original) - 16 + len(extra))
    with pytest.raises(ValueError, match="data after the zlib stream"):
        wesys_unpack.decode_wesys(data)


def test_false_size_decompression_bomb_stays_bounded():
    data = set_size(wrapped(b"x" * (2 * 1024**2)), 12, 64)
    tracemalloc.start()
    try:
        with pytest.raises(ValueError, match="exceeds.*declared size"):
            wesys_unpack.decode_wesys(data)
        _, peak = tracemalloc.get_traced_memory()
    finally:
        tracemalloc.stop()
    assert peak < 512 * 1024  # Inflating all 2 MiB would violate this bound.


def test_caps_actual_input_and_declared_output(tmp_path, monkeypatch):
    data = wrapped(b"abc" * 10)
    archive = tmp_path / "test.bin"
    archive.write_bytes(data)
    monkeypatch.setattr(wesys_unpack, "MAX_INPUT_SIZE", len(data) - 1)
    with pytest.raises(ValueError, match="input exceeds"):
        wesys_unpack.decode_wesys(data)
    with pytest.raises(ValueError, match="input exceeds"):
        wesys_unpack.inspect_wesys(archive)
    monkeypatch.setattr(wesys_unpack, "MAX_INPUT_SIZE", 1024)
    monkeypatch.setattr(wesys_unpack, "MAX_DECODED_SIZE", 29)
    with pytest.raises(ValueError, match="decoded size.*limit"):
        wesys_unpack.inspect_wesys(archive)


def test_bad_payload_creates_no_output_or_report(tmp_path):
    archive = tmp_path / "test.bin"
    original = set_size(wrapped(b"abc"), 12, 1)
    archive.write_bytes(original)
    output, report = tmp_path / "decoded.bin", tmp_path / "report.json"
    assert wesys_unpack.main(["--input", str(archive), "--output", str(output), "--json", str(report)]) == 1
    assert not output.exists() and not report.exists()
    assert archive.read_bytes() == original


def test_cli_decode_from_other_directory(tmp_path):
    archive, output, report = tmp_path / "input.bin", tmp_path / "decoded.bin", tmp_path / "report.json"
    archive.write_bytes(wrapped(b"synthetic"))
    result = subprocess.run([sys.executable, str(Path(wesys_unpack.__file__)), "--input", str(archive),
                             "--output", str(output), "--json", str(report)],
                            cwd=tmp_path, capture_output=True, text=True, timeout=5)
    assert result.returncode == 0
    assert result.stdout == ""
    assert output.read_bytes() == b"synthetic"
    metadata = json.loads(report.read_text())
    assert metadata["file_name"] == "input.bin"
    assert metadata["output_name"] == "decoded.bin"


def test_inspect_only_stdout(tmp_path, capsys):
    archive = tmp_path / "input.bin"
    archive.write_bytes(wrapped())
    assert wesys_unpack.main(["--input", str(archive)]) == 0
    metadata = json.loads(capsys.readouterr().out)
    assert "output_name" not in metadata
    assert list(tmp_path.iterdir()) == [archive]


@pytest.mark.parametrize("kind", ["missing", "directory", "fifo", "symlink"])
def test_input_cli_rejection_does_not_block(tmp_path, kind):
    archive = tmp_path / kind
    if kind == "directory":
        archive.mkdir()
    elif kind == "fifo":
        os.mkfifo(archive)
    elif kind == "symlink":
        target = tmp_path / "real.bin"
        target.write_bytes(wrapped())
        archive.symlink_to(target)
    result = subprocess.run([sys.executable, str(Path(wesys_unpack.__file__)), "--input", str(archive)],
                            capture_output=True, text=True, timeout=5)
    assert result.returncode == 1
    assert result.stdout == ""
    assert "Traceback" not in result.stderr


@pytest.mark.parametrize("kind", ["file", "symlink", "broken_symlink", "input"])
def test_output_overwrites_refused(tmp_path, kind):
    archive = tmp_path / "input.bin"
    original = wrapped()
    archive.write_bytes(original)
    output = tmp_path / "output.bin"
    if kind == "file":
        output.write_bytes(b"keep")
    elif kind == "input":
        output = archive
    elif kind == "symlink":
        output.symlink_to(archive)
    else:
        output.symlink_to(tmp_path / "missing")
    with pytest.raises(ValueError, match="already exists"):
        wesys_unpack.inspect_wesys(archive, output=output)
    assert archive.read_bytes() == original
    if kind == "file":
        assert output.read_bytes() == b"keep"


def test_destination_checks_before_decoding(tmp_path):
    archive = tmp_path / "input.bin"
    archive.write_bytes(wrapped())
    output, report = tmp_path / "decoded.bin", tmp_path / "report.json"
    args = ["--input", str(archive), "--output", str(output)]
    report.write_text("keep")
    assert wesys_unpack.main(args + ["--json", str(report)]) == 1
    assert report.read_text() == "keep" and not output.exists()
    assert wesys_unpack.main(args + ["--json", str(output)]) == 1
    assert not output.exists()
    assert wesys_unpack.main(args + ["--json", str(tmp_path / "missing" / "report")]) == 1
    assert not output.exists()


def test_input_mutation_creates_no_decoded_output(tmp_path, monkeypatch):
    archive, output = tmp_path / "input.bin", tmp_path / "decoded.bin"
    archive.write_bytes(wrapped())
    original = os.fstat
    calls = 0
    def append_after_read(fd):
        nonlocal calls
        calls += 1
        if calls == 2:
            with archive.open("ab") as changed:
                changed.write(b"extra")
        return original(fd)
    monkeypatch.setattr(os, "fstat", append_after_read)
    with pytest.raises(ValueError, match="input changed"):
        wesys_unpack.inspect_wesys(archive, output=output)
    assert not output.exists()
