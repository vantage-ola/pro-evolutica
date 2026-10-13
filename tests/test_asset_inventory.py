import csv
import json
import os
from pathlib import Path
import subprocess
import sys

import pytest

from tools import asset_inventory


def test_recursive_metadata_and_magic_bounds(tmp_path):
    (tmp_path / "nested").mkdir()
    (tmp_path / "nested" / "stadium.BIN").write_bytes(bytes(range(100)))
    (tmp_path / "empty").touch()
    (tmp_path / "short.dat").write_bytes(b"AB")
    report = asset_inventory.inventory(tmp_path)
    assert report["file_count"] == 3
    assert report["total_size_bytes"] == 102
    assert report["complete"] is True
    assert report["errors"] == []
    assert report["files"] == [
        {"path": "empty", "size_bytes": 0, "extension": "", "magic_hex": ""},
        {"path": "nested/stadium.BIN", "size_bytes": 100, "extension": ".bin", "magic_hex": bytes(range(16)).hex()},
        {"path": "short.dat", "size_bytes": 2, "extension": ".dat", "magic_hex": "4142"},
    ]


def test_empty_root(tmp_path):
    report = asset_inventory.inventory(tmp_path)
    assert report["files"] == []
    assert report["total_size_bytes"] == 0


def test_skips_symlinks_cycles_and_special_files(tmp_path):
    root = tmp_path / "root"
    root.mkdir()
    outside = tmp_path / "outside"
    outside.write_text("private")
    (root / "escape").symlink_to(outside)
    (root / "cycle").symlink_to(root, target_is_directory=True)
    (root / "broken").symlink_to(root / "absent")
    os.mkfifo(root / "pipe")
    report = asset_inventory.inventory(root)
    assert report["files"] == []
    assert [item["path"] for item in report["skipped"]] == ["broken", "cycle", "escape", "pipe"]
    assert report["errors"] == []


@pytest.mark.parametrize("kind", ["missing", "file", "symlink"])
def test_invalid_roots(tmp_path, kind):
    root = tmp_path / kind
    if kind == "file":
        root.touch()
    elif kind == "symlink":
        root.symlink_to(tmp_path, target_is_directory=True)
    with pytest.raises(ValueError, match="root"):
        asset_inventory.inventory(root)


def test_records_unreadable_files_and_continues(tmp_path, monkeypatch):
    (tmp_path / "bad.bin").write_bytes(b"bad")
    (tmp_path / "good.bin").write_bytes(b"good")
    original = os.open
    def deny_bad(path, *args, **kwargs):
        if str(path) == "bad.bin":
            raise PermissionError("test: permission denied")
        return original(path, *args, **kwargs)
    monkeypatch.setattr(os, "open", deny_bad)
    report = asset_inventory.inventory(tmp_path)
    assert report["complete"] is False
    assert report["file_count"] == 1
    assert report["errors"] == [{"path": "bad.bin", "error": "test: permission denied"}]


def test_records_unreadable_subdirectories(tmp_path, monkeypatch):
    (tmp_path / "locked").mkdir()
    original = os.open
    def deny_locked(path, *args, **kwargs):
        if Path(path).name == "locked":
            raise PermissionError("test: directory denied")
        return original(path, *args, **kwargs)
    monkeypatch.setattr(os, "open", deny_locked)
    report = asset_inventory.inventory(tmp_path)
    assert report["complete"] is False
    assert report["errors"][0]["path"] == "locked"


def test_entry_limit(tmp_path, monkeypatch):
    (tmp_path / "a").touch()
    (tmp_path / "b").touch()
    monkeypatch.setattr(asset_inventory, "MAX_ENTRIES", 1)
    with pytest.raises(ValueError, match="entry limit"):
        asset_inventory.inventory(tmp_path)


def test_replaced_parent_directory_cannot_escape_root(tmp_path, monkeypatch):
    root, outside = tmp_path / "root", tmp_path / "outside"
    (root / "a" / "b").mkdir(parents=True)
    (outside / "b").mkdir(parents=True)
    (outside / "b" / "secret").write_text("do not read")
    original = os.open
    visits = 0
    def replace_parent(path, *args, **kwargs):
        nonlocal visits
        if str(path) == "a":
            visits += 1
            if visits == 2:
                (root / "a").rename(root / "moved")
                (root / "a").symlink_to(outside, target_is_directory=True)
        return original(path, *args, **kwargs)
    monkeypatch.setattr(os, "open", replace_parent)
    report = asset_inventory.inventory(root)
    assert report["files"] == []
    assert report["complete"] is False
    assert report["errors"][0]["path"] == "a/b"


def test_json_and_csv_agree_and_quote_names(tmp_path):
    root = tmp_path / "inputs"
    root.mkdir()
    name = 'café,"pitch".BIN'
    (root / name).write_bytes(b"\0\xFF")
    (root / "link").symlink_to(root / name)
    json_path, csv_path = tmp_path / "report.json", tmp_path / "report.csv"
    assert asset_inventory.main(["--root", str(root), "--json", str(json_path), "--csv", str(csv_path)]) == 0
    report = json.loads(json_path.read_text())
    with csv_path.open(newline="", encoding="utf-8") as stream:
        rows = list(csv.DictReader(stream))
    assert rows[0]["path"] == report["files"][0]["path"] == name
    assert int(rows[0]["size_bytes"]) == report["files"][0]["size_bytes"] == 2
    assert rows[0]["magic_hex"] == "00ff"
    assert rows[1]["status"] == "skipped"


def test_incomplete_cli_returns_failure_and_saves_errors(tmp_path, monkeypatch, capsys):
    root = tmp_path / "inputs"
    root.mkdir()
    (root / "bad.bin").touch()
    original = os.open
    def deny_bad(path, *args, **kwargs):
        if str(path) == "bad.bin":
            raise PermissionError("denied")
        return original(path, *args, **kwargs)
    monkeypatch.setattr(os, "open", deny_bad)
    output = tmp_path / "partial.json"
    assert asset_inventory.main(["--root", str(root), "--json", str(output)]) == 1
    assert json.loads(output.read_text())["complete"] is False
    assert "incomplete" in capsys.readouterr().err


def test_refuses_overwrites_and_duplicate_destinations(tmp_path):
    root = tmp_path / "inputs"
    root.mkdir()
    original = root / "game.bin"
    original.write_bytes(b"original")
    assert asset_inventory.main(["--root", str(root), "--json", str(original)]) == 1
    assert original.read_bytes() == b"original"
    output = tmp_path / "report"
    assert asset_inventory.main(["--root", str(root), "--json", str(output), "--csv", str(output)]) == 1
    assert not output.exists()


def test_cli_script_and_csv_only_and_missing_root(tmp_path):
    script = str(Path(asset_inventory.__file__))
    output = tmp_path / "report.csv"
    result = subprocess.run([sys.executable, script, "--root", str(tmp_path), "--csv", str(output)],
                            cwd=tmp_path, capture_output=True, text=True, timeout=5)
    assert result.returncode == 0
    assert result.stdout == ""
    assert output.read_text().startswith("path,status,size_bytes,extension,magic_hex,error")
    result = subprocess.run([sys.executable, script, "--root", str(tmp_path / "missing")],
                            capture_output=True, text=True, timeout=5)
    assert result.returncode == 1
    assert "Traceback" not in result.stderr
