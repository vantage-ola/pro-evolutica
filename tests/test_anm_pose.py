"""Known original joint/weight poses; no game inputs or reference implementation."""

import json
import math
import struct

import pytest

from tools import anm_pose as pose
from test_skeletal_inspection import IDENTITY, animation_fixture, model_fixture


def pack_rotation(q, missing=3):
    # Original fixture encoder; nearest quantized coordinates in the documented layout.
    values = [round(q[i]*23169.767578125+16383.5) for i in range(4) if i != missing]
    return (missing | values[0]<<32 | values[1]<<17 | values[2]<<2).to_bytes(6,"little")


def clip_fixture():
    b = animation_fixture()
    root = pack_rotation((0,0,0,1))
    child = pack_rotation((0,0,math.sqrt(.5),math.sqrt(.5)))
    for i in range(3):
        b[152+6*i:158+6*i] = root
        b[256+6*i:262+6*i] = child
    struct.pack_into("<9f",b,192,*([0.]*9))
    return b


def test_literal_identity_packing():
    # Low two bits select W; three integer fields are each 16384.
    q = pose.packed_quaternion(bytes.fromhex("030001800040"))
    assert q[:3] == pytest.approx([.5/23169.767578125]*3,abs=1e-15)
    assert q[3] == pytest.approx(math.sqrt(1-3*(.5/23169.767578125)**2),abs=1e-15)


@pytest.mark.parametrize("missing",range(4))
def test_each_omitted_component_and_sign(missing):
    q = [-.1,.2,-.3,.4]
    q[missing] = math.sqrt(1-sum(q[i]**2 for i in range(4) if i != missing))
    decoded = pose.packed_quaternion(pack_rotation(q,missing))
    assert decoded == pytest.approx(q,abs=4e-5)
    assert sum(x*x for x in decoded) == pytest.approx(1,abs=1e-15)


@pytest.mark.parametrize("axis",range(3))
def test_known_right_angle_rotation(axis):
    q = [0,0,0,math.sqrt(.5)]
    q[axis] = math.sqrt(.5)
    v = [[0,1,0],[0,0,1],[1,0,0]][axis]
    expected = [[0,0,1],[1,0,0],[0,1,0]][axis]
    assert pose.point(v,pose.rotation_matrix(q)) == pytest.approx(expected,abs=1e-15)
    assert pose.point(v,pose.rotation_matrix(pose.packed_quaternion(pack_rotation(q)))) == pytest.approx(expected,abs=1e-4)


@pytest.mark.parametrize("payload",[b"",b"12345",b"1234567",(1<<47).to_bytes(6,"little"),((32767<<32)|(32767<<17)|(32767<<2)).to_bytes(6,"little")])
def test_invalid_packed_rotations(payload):
    with pytest.raises(ValueError):
        pose.packed_quaternion(payload)


@pytest.mark.parametrize("order",["first","last"])
@pytest.mark.parametrize("kind",[None,0,1,2])
def test_bind_pose_recovers_source_geometry(kind,order):
    report,mesh = pose.evaluate_pose(model_fixture(kind),weight_order=order)
    assert report["max_vertex_displacement"] == 0
    assert mesh["packets"][0]["positions"] == [[0,0,0],[1,0,0],[0,1,0]]
    assert mesh["packets"][0]["triangles"] == [[0,1,2]]


def test_absolute_local_rotation_about_translated_child_joint():
    model = model_fixture(0)
    struct.pack_into("<3f",model,768,3,0,0)
    for order,expected in [("first",[2.75,.25,0]),("last",[2.25,.75,0])]:
        report,mesh = pose.evaluate_pose(model,clip_fixture(),sample=1,core_offset=64,sequence_offset=0,weight_order=order)
        assert mesh["packets"][0]["positions"][0] == pytest.approx(expected,abs=2e-4)
        assert report["global_bone_matrices"][1][12:15] == pytest.approx([2,0,0],abs=2e-4)
        assert report["sample_seconds"] == 1/30
    # A child rotation is absolute local, not multiplied by its rest rotation.
    model = model_fixture(0)
    a = pose.rotation_matrix([0,0,math.sqrt(.5),math.sqrt(.5)])
    a[12:15] = [2,0,0]
    inverse = [a[c*4+r] if r<3 and c<3 else IDENTITY[r*4+c] for r in range(4) for c in range(4)]
    inverse[12:15] = [-sum(a[12+k]*inverse[k*4+c] for k in range(3)) for c in range(3)]
    struct.pack_into("<16f",model,384,*a)
    struct.pack_into("<16f",model,448,*inverse)
    report,mesh = pose.evaluate_pose(model,clip_fixture(),core_offset=64,weight_order="first")
    assert report["max_vertex_displacement"] < 2e-4


def test_parent_rotation_and_root_translation_propagate_once():
    clip = clip_fixture()
    for i in range(3):
        clip[152+i*6:158+i*6] = pack_rotation((0,0,math.sqrt(.5),math.sqrt(.5)))
        clip[256+i*6:262+i*6] = pack_rotation((0,0,0,1))
    struct.pack_into("<3f",clip,192,10,20,30)
    report,mesh = pose.evaluate_pose(model_fixture(None),clip,core_offset=64,weight_order="first")
    assert report["global_bone_matrices"][1][12:15] == pytest.approx([10,22,30],abs=2e-4)
    assert mesh["packets"][0]["positions"][1] == pytest.approx([10,21,30],abs=2e-4)
    assert report["fps"] is None and report["sample_seconds"] is None


def test_parent_may_follow_child_in_table():
    model = model_fixture()
    struct.pack_into("<i",model,364,1)
    struct.pack_into("<i",model,540,-1)
    report,_ = pose.evaluate_pose(model,weight_order="first")
    assert report["max_vertex_displacement"] == 0


def test_palette_permutation_repeated_indices_and_rounding():
    raw = {"explicit_weights":[.25,.5],"palette_indices":[0,1,0,255]}
    assert pose.influences(raw,[1,0],"first") == [(1,.25),(0,.25),(1,.5)]
    assert pose.influences(raw,[1,0],"last") == [(1,.25),(0,.5),(1,.25)]
    raw["explicit_weights"] = [.5,.500001]
    assert sum(w for _,w in pose.influences(raw,[1,0],"first")) == pytest.approx(1)


@pytest.mark.parametrize("offset,value",[(208,2),(220,.1),(268,-1),(288,2),(104,1),(132,1)])
def test_reject_nonrigid_nonaffine_wrong_inverse_or_morph(offset,value):
    model = model_fixture()
    struct.pack_into("<I" if offset in (104,132) else "<f",model,offset,value)
    with pytest.raises(ValueError):
        pose.evaluate_pose(model,weight_order="first")


def test_reflected_bind_is_refused_even_with_correct_inverse():
    model = model_fixture()
    struct.pack_into("<f",model,208,-1)
    struct.pack_into("<f",model,272,-1)
    with pytest.raises(ValueError,match="rigid affine"):
        pose.evaluate_pose(model,weight_order="first")


@pytest.mark.parametrize("sample",[-1,3,1.5,True])
def test_reject_out_of_range_or_noninteger_sample(sample):
    with pytest.raises(ValueError,match="sample"):
        pose.evaluate_pose(model_fixture(),clip_fixture(),core_offset=64,sample=sample,weight_order="first")


def test_reject_incomplete_rotation_tracks_missing_influences_and_mismatch():
    clip = clip_fixture()
    struct.pack_into("<H",clip,240,0x1d) # Duplicate child translation / truncated payload.
    with pytest.raises(ValueError):
        pose.evaluate_pose(model_fixture(),clip,core_offset=64,weight_order="first")
    model = model_fixture()
    struct.pack_into("<i",model,540,-1)
    with pytest.raises(ValueError,match="match"):
        pose.evaluate_pose(model,clip_fixture(),core_offset=64,weight_order="first")
    model = model_fixture(None)
    model[727] = 17 # No blend indices; previously UBYTE4 at declaration 1.
    with pytest.raises(ValueError,match="blend indices"):
        pose.evaluate_pose(model,weight_order="first")
    with pytest.raises(ValueError,match="weight order"):
        pose.evaluate_pose(model_fixture(),weight_order="guess")
    with pytest.raises(ValueError,match="animation"):
        pose.evaluate_pose(model_fixture(),sample=1,weight_order="first")


def test_obj_indices_across_packets():
    packet = {"positions":[[0,0,0],[1,0,0],[0,1,0]],"triangles":[[0,1,2]]}
    text = pose.obj_text({"packets":[packet,packet]})
    assert "f 1 2 3\n" in text and "f 4 5 6\n" in text


def test_valid_clip_with_missing_rotation_is_rejected():
    clip = clip_fixture()
    # Keep only the root rotation and translation, with a shorter valid offset table.
    struct.pack_into("<I",clip,120,20)
    struct.pack_into("<II",clip,124,64,0)
    clip[132:136] = clip[136:140]
    # Move the root track back four bytes, clearing its old final padding.
    clip[132:166] = clip[136:170]
    clip[166:176] = bytes(10)
    with pytest.raises(ValueError,match="every bone"):
        pose.evaluate_pose(model_fixture(),clip[:228],core_offset=64,weight_order="first")


def test_index_cap_and_bind_offset_options(monkeypatch):
    monkeypatch.setattr(pose,"MAX_EXPORT_INDICES",2)
    with pytest.raises(ValueError,match="index limit"):
        pose.evaluate_pose(model_fixture(),weight_order="first")
    monkeypatch.setattr(pose,"MAX_EXPORT_INDICES",200_000)
    for options in ({"core_offset":64},{"sequence_offset":0}):
        with pytest.raises(ValueError,match="animation"):
            pose.evaluate_pose(model_fixture(),weight_order="first",**options)


def test_cli_output_protection_and_errors(tmp_path,capsys):
    model,clip = tmp_path/'original.ktmdl',tmp_path/'original.anm'
    model.write_bytes(model_fixture());clip.write_bytes(clip_fixture())
    obj,report = tmp_path/'pose.obj',tmp_path/'pose.json'
    args = ['--model',str(model),'--animation',str(clip),'--core-offset','64',
            '--sequence-offset','0','--sample','2','--weight-order','first','--obj',str(obj),'--json',str(report)]
    assert pose.main(args) == 0
    assert json.loads(report.read_text())["sample_seconds"] == 2/30
    before = obj.read_bytes(),report.read_bytes()
    assert pose.main(args) == 1 and before == (obj.read_bytes(),report.read_bytes())
    obj.unlink();report.unlink()
    report.symlink_to(tmp_path/'absent')
    assert pose.main(args) == 1 and not obj.exists()
    report.unlink()
    assert pose.main(args[:-1]+[str(obj)]) == 1 and not obj.exists()
    model.unlink();model.symlink_to(clip)
    assert pose.main(args) == 1 and not obj.exists()
    model.unlink()
    assert pose.main(args) == 1
    model.write_bytes(model_fixture())
    assert pose.main(['--model',str(model),'--weight-order','last','--obj',str(obj)]) == 0
    assert '"animation_sha256": null' in capsys.readouterr().out
