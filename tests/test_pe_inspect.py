import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys

import pytest

from tools import pe_inspect
from conftest import make_pe


@pytest.mark.parametrize("bits,architecture,image_base", [(32, "x86", 0x400000), (64, "x86_64", 0x140000000)])
def test_metadata_and_named_and_ordinal_imports(pe_factory, bits, architecture, image_base):
    path = pe_factory(bits=bits)
    report = pe_inspect.inspect_pe(path)
    assert report["sha256"] == hashlib.sha256(path.read_bytes()).hexdigest()
    assert report["file_size"] == 0x800
    assert report["file_name"] == "synthetic.exe"
    assert report["architecture"] == architecture
    assert report["bits"] == bits
    assert report["entry_point"] == {"rva": 0x1000, "va": image_base + 0x1000}
    assert [s["name"] for s in report["sections"]] == [".text", ".idata"]
    assert report["sections"][1]["raw_offset"] == 0x400
    assert report["imports"] == [{"dll": "KERNEL32.dll", "kind": "standard", "functions": [
        {"name": "ExitProcess", "ordinal": None}, {"name": None, "ordinal": 7}]}]
    assert report["warnings"] == []


@pytest.mark.parametrize("bits", [32, 64])
def test_delayed_imports(pe_factory, bits):
    report = pe_inspect.inspect_pe(pe_factory(bits=bits, delay=True))
    assert report["imports"][1] == {"dll": "USER32.dll", "kind": "delay", "functions": [
        {"name": "MessageBoxA", "ordinal": None}]}


def test_no_imports_and_unknown_machine(pe_factory):
    report = pe_inspect.inspect_pe(pe_factory(imports=False, machine=0xFFFF))
    assert report["imports"] == []
    assert report["architecture"] == "unknown"
    assert report["machine"] == "0xffff"


def test_arm64(pe_factory):
    assert pe_inspect.inspect_pe(pe_factory(bits=64, machine=0xAA64))["architecture"] == "arm64"


@pytest.mark.parametrize("data", [b"", b"hello", b"MZ" + bytes(100), make_pe()[:0x170], make_pe()[:-1]])
def test_rejects_invalid_and_truncated_files(tmp_path, data):
    path = tmp_path / "bad.exe"
    path.write_bytes(data)
    with pytest.raises(ValueError):
        pe_inspect.inspect_pe(path)


@pytest.mark.parametrize("offset,fmt,value", [
    (60, "<I", 0xFFFFFFFF), (0x86, "<H", 65535), (0x98, "<H", 0x107),
    (0x94, "<H", 1), (0xF4, "<I", 17), (0x178 + 20, "<I", 0xFFFFFFF0),
    (0x104, "<I", 0xFFFFF000),
])
def test_rejects_out_of_bounds_header_fields(tmp_path, offset, fmt, value):
    data = bytearray(make_pe())
    struct.pack_into(fmt, data, offset, value)
    path = tmp_path / "corrupt.exe"
    path.write_bytes(data)
    with pytest.raises(ValueError):
        pe_inspect.inspect_pe(path)


def test_limits_file_allocation(pe_factory, monkeypatch):
    path = pe_factory()
    monkeypatch.setattr(pe_inspect, "MAX_FILE_BYTES", 100)
    with pytest.raises(ValueError, match="inspection limit"):
        pe_inspect.inspect_pe(path)


def test_limits_import_descriptor_count(pe_factory, monkeypatch):
    monkeypatch.setattr(pe_inspect, "MAX_IMPORT_DLLS", 0)
    with pytest.raises(ValueError, match="exceeds limit"):
        pe_inspect.inspect_pe(pe_factory())


def test_requires_import_terminator_within_declared_size(tmp_path):
    data = bytearray(make_pe())
    struct.pack_into("<I", data, 0x104, 20)
    path = tmp_path / "unterminated.exe"
    path.write_bytes(data)
    with pytest.raises(ValueError, match="terminator"):
        pe_inspect.inspect_pe(path)


def test_uses_iat_when_lookup_table_is_absent(tmp_path):
    data = bytearray(make_pe())
    struct.pack_into("<I", data, 0x400, 0)
    path = tmp_path / "iat.exe"
    path.write_bytes(data)
    report = pe_inspect.inspect_pe(path)
    assert report["imports"][0]["functions"][0]["name"] == "ExitProcess"


def test_bad_import_symbols_are_reported_as_warnings(tmp_path):
    data = bytearray(make_pe())
    struct.pack_into("<I", data, 0x450, 0x7FFFFFFF)
    struct.pack_into("<I", data, 0x470, 0x7FFFFFFF)
    path = tmp_path / "bad-import.exe"
    path.write_bytes(data)
    assert pe_inspect.inspect_pe(path)["warnings"]


def test_cli_json_and_stdout_and_input_preservation(pe_factory, tmp_path, capsys):
    path = pe_factory()
    original = path.read_bytes()
    report_path = tmp_path / "report.json"
    assert pe_inspect.main(["--input", str(path), "--json", str(report_path)]) == 0
    assert json.loads(report_path.read_text())["sha256"] == hashlib.sha256(original).hexdigest()
    assert pe_inspect.main(["--input", str(path)]) == 0
    assert json.loads(capsys.readouterr().out)["bits"] == 32
    assert pe_inspect.main(["--input", str(path), "--json", str(path)]) == 1
    assert path.read_bytes() == original
    assert pe_inspect.main(["--input", str(path), "--json", str(report_path)]) == 1


@pytest.mark.parametrize("kind", ["missing", "directory", "non-pe", "fifo"])
def test_cli_invalid_input_has_no_traceback(tmp_path, kind):
    path = tmp_path / kind
    if kind == "directory":
        path.mkdir()
    elif kind == "non-pe":
        path.write_text("hello")
    elif kind == "fifo":
        import os
        os.mkfifo(path)
    result = subprocess.run([sys.executable, str(Path(pe_inspect.__file__)), "--input", str(path)],
                            capture_output=True, text=True, timeout=5)
    assert result.returncode == 1
    assert "pe_inspect:" in result.stderr
    assert "Traceback" not in result.stderr
    assert result.stdout == ""


def test_cli_can_run_from_another_directory(pe_factory, tmp_path):
    result = subprocess.run([sys.executable, str(Path(pe_inspect.__file__)), "--input", str(pe_factory())],
                            cwd=tmp_path, capture_output=True, text=True, timeout=5)
    assert result.returncode == 0
    assert json.loads(result.stdout)["bits"] == 32


def test_unwritable_report_destination(pe_factory, tmp_path, capsys):
    path = tmp_path / "missing" / "report.json"
    assert pe_inspect.main(["--input", str(pe_factory()), "--json", str(path)]) == 1
    assert "pe_inspect:" in capsys.readouterr().err
    assert not path.exists()
