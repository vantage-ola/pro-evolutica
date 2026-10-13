"""Original geometry fixtures, independent of commercial game files."""

import json
import math
import struct

import pytest

from tools import ktmdl_inspect as model


def fixture(minor=1):
    data = bytearray(672)
    data[:8] = b"KTMDL\0\0\0"
    struct.pack_into("<II", data, 8, 2, minor)
    data[16:18] = b"\x01\x00"
    struct.pack_into("<Ii", data, 40, 1, 192)  # One 96-byte packet.
    struct.pack_into("<Ii", data, 48, 2, 288)  # Two stream descriptors.
    struct.pack_into("<Ii", data, 136, 3, 352)  # Position/normal/UV declaration.
    struct.pack_into("<Ii", data, 116, 144, 528)
    struct.pack_into("<I", data, 144, len(data))
    struct.pack_into("<Ii", data, 192 + 16, 1, 96)
    struct.pack_into("<Ii", data, 192 + 32, 1, 128)
    struct.pack_into("<iI4Bi", data, 288, 240, 4, 1, 32, 3, 1, 64)
    struct.pack_into("<iI", data, 320, 336, 4)
    struct.pack_into("<i", data, 320 + 20, 1)
    data[352:364] = bytes([0,0,2,16, 0,12,2,18, 0,24,1,22])
    for i, position in enumerate(((0,0,0),(2,0,0),(0,3,0),(2,3,0))):
        struct.pack_into("<8f", data, 528+i*32, *position, 0,0,1, i%2, i//2)
    struct.pack_into("<4H", data, 656, 0,1,2,3)
    return data


@pytest.mark.parametrize("minor", [1,2])
def test_metadata(minor):
    report = model.inspect_model(fixture(minor))
    assert report["version"] == [2,minor]
    assert report["packet_count"] == 1
    assert report["vertex_count"] == report["index_count"] == 4
    packet = report["packets"][0]
    assert packet["position_bounds_min"] == [0,0,0]
    assert packet["position_bounds_max"] == [2,3,0]
    assert packet["triangle_count_without_repeated_indices"] == 2
    assert packet["restart_count"] == packet["repeated_index_windows"] == 0
    assert packet["attributes"] == [{"offset":0,"type":2,"semantic":16},
                                    {"offset":12,"type":2,"semantic":18},
                                    {"offset":24,"type":1,"semantic":22}]
    assert "triangle area" in report["limitations"]


@pytest.mark.parametrize("size", [0,7,8,16,191,192,287,319,351,527,655,671])
def test_truncated(size):
    with pytest.raises(ValueError):
        model.inspect_model(fixture()[:size])


@pytest.mark.parametrize("offset,fmt,value", [
    (0,"B",0), (8,"I",3), (12,"I",3), (16,"H",256), (144,"I",700),
    (40,"I",0), (40,"I",4097), (44,"i",-1), (44,"i",128), (44,"i",640),
    (48,"I",0), (48,"I",4097), (52,"i",192), (136,"I",0), (140,"i",670),
    (116,"I",145), (120,"i",530), (194,"B",1), (208,"I",2), (224,"I",0),
    (212,"i",97), (228,"i",500), (288,"i",0), (292,"I",0), (292,"I",1_000_001),
    (297,"B",0), (297,"B",24), (298,"B",0), (300,"i",-100), (308,"i",1),
    (320,"i",352), (324,"I",2_000_001), (340,"i",0), (352,"B",1),
    (354,"B",255), (354,"B",1), (355,"B",18), (361,"B",31), (363,"B",18),
    (656,"H",4),
])
def test_bad_fields(offset, fmt, value):
    data = fixture()
    struct.pack_into("<"+fmt, data, offset, value)
    with pytest.raises(ValueError):
        model.inspect_model(data)


@pytest.mark.parametrize("value", [math.nan,math.inf,-math.inf])
def test_nonfinite_positions(value):
    data = fixture()
    struct.pack_into("<f", data, 528, value)
    with pytest.raises(ValueError, match="non-finite"):
        model.inspect_model(data)


def test_strip_restarts_and_repeated_indices():
    data = fixture()
    struct.pack_into("<I", data, 324, 8)
    struct.pack_into("<8H", data, 656, 0,1,2,65535,0,0,2,3)
    packet = model.inspect_model(data)["packets"][0]
    assert packet["restart_count"] == 1
    assert packet["repeated_index_windows"] == 1
    assert packet["triangle_count_without_repeated_indices"] == 2


def test_size_limit(monkeypatch):
    monkeypatch.setattr(model, "MAX_INPUT_SIZE", 671)
    with pytest.raises(ValueError, match="limit"):
        model.inspect_model(fixture())


@pytest.mark.parametrize("limit", ["MAX_VERTICES","MAX_INDICES"])
def test_aggregate_limit(monkeypatch, limit):
    # Reuse valid bounded descriptors from two packets; the aggregate cap must
    # still count both references. The packet/data tables remain disjoint.
    original = fixture()
    data = bytearray(864)
    data[:192] = original[:192]
    data[288:364] = original[288:364]  # Descriptors and declarations.
    data[720:864] = original[528:672]  # Original geometry bytes.
    for packet in (368,464):
        data[packet:packet+96] = original[192:288]
        struct.pack_into("<i", data, packet+20, 288-packet)
        struct.pack_into("<i", data, packet+36, 320-packet)
    struct.pack_into("<Ii", data, 40, 2, 368)
    struct.pack_into("<Ii", data, 116, 144, 720)
    struct.pack_into("<i", data, 288, 720-288)
    struct.pack_into("<i", data, 320, 848-320)
    struct.pack_into("<I", data, 144, len(data))
    monkeypatch.setattr(model, limit, 6)
    with pytest.raises(ValueError, match="aggregate"):
        model.inspect_model(data)


def test_file_and_cli(tmp_path, capsys):
    source = tmp_path / "original.ktmdl"
    source.write_bytes(fixture())
    output = tmp_path / "report.json"
    assert model.main(["--input",str(source),"--json",str(output)]) == 0
    assert json.loads(output.read_text())["file_name"] == source.name
    before = output.read_bytes()
    assert model.main(["--input",str(source),"--json",str(output)]) == 1
    assert output.read_bytes() == before
    assert "already exists" in capsys.readouterr().err
    assert model.main(["--input",str(source)]) == 0
    assert json.loads(capsys.readouterr().out)["format"] == "KTMDL"
    assert model.main(["--input",str(tmp_path / "missing")]) == 1


def test_symlink_and_directory(tmp_path):
    source = tmp_path / "original.ktmdl"
    source.write_bytes(fixture())
    link = tmp_path / "link"
    link.symlink_to(source)
    with pytest.raises(OSError):
        model.inspect_ktmdl(link)
    with pytest.raises((OSError,ValueError)):
        model.inspect_ktmdl(tmp_path)


def test_fifo(tmp_path):
    import os
    fifo = tmp_path / "fifo"
    os.mkfifo(fifo)
    with pytest.raises(ValueError, match="regular file"):
        model.inspect_ktmdl(fifo)


def test_changed_file(tmp_path, monkeypatch):
    source = tmp_path / "original.ktmdl"
    source.write_bytes(fixture())
    original = model.os.fstat
    calls = 0
    def changing(fd):
        nonlocal calls
        calls += 1
        result = original(fd)
        if calls == 2:
            from types import SimpleNamespace
            return SimpleNamespace(st_size=result.st_size+1, st_mtime_ns=result.st_mtime_ns,
                                   st_ctime_ns=result.st_ctime_ns)
        return result
    monkeypatch.setattr(model.os, "fstat", changing)
    with pytest.raises(ValueError, match="changed"):
        model.inspect_ktmdl(source)
