"""Original static fixtures with known faces, normals, UVs and texture IDs."""

import json
import math
from pathlib import Path
import struct
import subprocess
import sys

import pytest

from tools import ktmdl_export as export
from test_ktmdl_inspect import fixture


def static_fixture():
    original = fixture(2)
    data = bytearray(1040)
    data[:192] = original[:192]
    struct.pack_into("<Ii", data, 24, 1,192)
    struct.pack_into("<Ii", data, 32, 1,368)
    struct.pack_into("<16f", data, 208, *export.IDENTITY)
    struct.pack_into("<16f", data, 272, *export.IDENTITY)
    struct.pack_into("<i", data, 364, -1)
    data[384:480] = original[192:288]
    data[480:544] = original[288:352]
    data[544:556] = original[352:364]
    struct.pack_into("<Ii", data, 40, 1,384)
    struct.pack_into("<Ii", data, 48, 2,480)
    struct.pack_into("<Ii", data, 136, 3,544)
    struct.pack_into("<Ii", data, 116, 136,896)
    struct.pack_into("<I", data, 144, len(data))
    struct.pack_into("<i", data, 480, 416)
    struct.pack_into("<i", data, 512, 512)
    struct.pack_into("<Ii", data, 408, 1,-16)
    struct.pack_into("<Ii", data, 72, 1,576)
    struct.pack_into("<Ii", data, 124, 1,736)
    struct.pack_into("<QQ", data, 736, 0,2)
    struct.pack_into("<Ii", data, 80, 1,752)
    struct.pack_into("<4f", data, 768, 1,1,0,0)
    struct.pack_into("<i", data, 96, 832)
    struct.pack_into("<i", data, 836, 20)
    struct.pack_into("<I", data, 852, 4)
    data[856:869] = b"original.png\0"
    data[428] = 1
    data[896:1032] = original[528:664]
    return data


def test_known_geometry_and_obj():
    report, mesh = export.decode_static_model(static_fixture())
    assert mesh["attributes"][16] == [[0,0,0],[2,0,0],[0,3,0],[2,3,0]]
    assert mesh["attributes"][22] == [[0,0],[1,0],[0,1],[1,1]]
    assert mesh["indices"] == [0,1,2,3]
    assert mesh["triangles"] == [[0,1,2],[2,1,3]]
    g = report["static_geometry"]
    assert g["triangle_area_min"] == g["triangle_area_max"] == 3
    assert g["faces_aligned_with_normals"] == 2
    assert g["faces_opposed_to_normals"] == g["faces_perpendicular_to_normals"] == 0
    assert g["bone_transforms"] == "identity"
    assert g["texture_references"][0]["name"] == "original.png"
    assert g["texture_references"][0]["id_lo"] == 2
    lines = export.obj_text(mesh).splitlines()
    assert [s for s in lines if s.startswith("f ")] == ["f 1/1/1 2/2/2 3/3/3", "f 3/3/3 2/2/2 4/4/4"]
    assert "v 2 3 0" in lines and "vt 0 1" in lines and lines.count("vn 0 0 1") == 4


@pytest.mark.parametrize("minor", [1,2])
def test_plain_static_packet_without_bones_or_textures(minor):
    report, _ = export.decode_static_model(fixture(minor))
    assert report["static_geometry"]["bone_transforms"] == "none"
    assert report["static_geometry"]["texture_references"] == []


@pytest.mark.parametrize("indices,faces", [
    ([0,1,2,3], [[0,1,2],[2,1,3]]),
    ([0,0,1,2], [[1,0,2]]),
    ([0,1,2,2,3,4], [[0,1,2],[3,2,4]]),
    ([0,1,2,65535,3,4,5,6], [[0,1,2],[3,4,5],[5,4,6]]),
    ([65535,65535,0,1], []),
])
def test_strip_parity_across_degenerates_and_restarts(indices, faces):
    assert export.strip_triangles(indices) == faces


@pytest.mark.parametrize("offset,fmt,value", [
    (24,"I",2), (28,"i",1040), (208,"f",2), (220,"f",1), (272,"f",2), (364,"i",0),
    (408,"I",2), (412,"i",0), (368,"H",1), (104,"I",1), (132,"I",1), (148,"I",1),
    (72,"I",4097), (76,"i",896), (80,"I",4097), (388,"H",1), (428,"B",9),
    (432,"H",1), (435,"B",1), (767,"H",1),
    (96,"i",1040), (96,"i",736), (836,"i",0), (852,"I",0), (852,"I",1000),
    (856,"B",255), (908,"f",math.nan), (920,"f",math.inf), (768,"f",math.nan),
    (547,"B",32), (551,"B",33), (551,"B",19), (555,"B",24),
])
def test_reject_unsupported_or_damaged_static_data(offset, fmt, value):
    data = static_fixture()
    struct.pack_into("<" + fmt, data, offset, value)
    with pytest.raises(ValueError):
        export.decode_static_model(data)


def test_geometry_degeneracy_and_normal_checks():
    data = fixture()
    struct.pack_into("<3f", data, 528+2*32, 1,0,0)
    with pytest.raises(ValueError, match="zero-area"):
        export.decode_static_model(data)
    data = fixture()
    struct.pack_into("<3f", data, 540, 0,0,0)
    with pytest.raises(ValueError, match="zero-length"):
        export.decode_static_model(data)
    data = fixture()
    struct.pack_into("<4H", data, 656, 0,0,0,0)
    with pytest.raises(ValueError, match="no non-degenerate"):
        export.decode_static_model(data)


def test_unterminated_name():
    data = static_fixture()
    data[856:896] = b"x"*40
    with pytest.raises(ValueError, match="unterminated"):
        export.decode_static_model(data)


@pytest.mark.parametrize("cap", ["MAX_EXPORT_VERTICES","MAX_EXPORT_INDICES"])
def test_export_limits(monkeypatch, cap):
    monkeypatch.setattr(export, cap, 3)
    with pytest.raises(ValueError, match="limits"):
        export.decode_static_model(fixture())


def test_cli_and_exclusive_outputs(tmp_path, capsys):
    source = tmp_path / "original.ktmdl"
    source.write_bytes(static_fixture())
    obj, report = tmp_path / "mesh.obj", tmp_path / "mesh.json"
    args = ["--input",str(source),"--obj",str(obj),"--json",str(report)]
    assert export.main(args) == 0
    assert json.loads(report.read_text())["static_geometry"]["triangle_count"] == 2
    before = obj.read_bytes(), report.read_bytes(), source.read_bytes()
    assert export.main(args) == 1
    assert before == (obj.read_bytes(),report.read_bytes(),source.read_bytes())
    assert "already exists" in capsys.readouterr().err
    assert export.main(["--input",str(source),"--obj",str(tmp_path/"same"),"--json",str(tmp_path/"same")]) == 1
    broken = tmp_path / "broken.ktmdl"
    broken.write_bytes(b"KTMDL")
    output = tmp_path / "bad.obj"
    assert export.main(["--input",str(broken),"--obj",str(output)]) == 1
    assert not output.exists()
    link = tmp_path / "link.obj"
    link.symlink_to(tmp_path/"absent")
    assert export.main(["--input",str(source),"--obj",str(link)]) == 1


def test_script_from_another_directory(tmp_path):
    source = tmp_path / "original.ktmdl"
    source.write_bytes(fixture())
    script = Path(export.__file__).resolve()
    result = subprocess.run([sys.executable,str(script),"--input",str(source),"--obj",str(tmp_path/"mesh.obj")],
                            cwd=tmp_path,capture_output=True,text=True,timeout=10)
    assert result.returncode == 0, result.stderr
    assert json.loads(result.stdout)["static_geometry"]["triangle_count"] == 2
