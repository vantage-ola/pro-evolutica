"""Original two-bone geometry and dense animation fixtures; no commercial inputs."""

import hashlib
import json
import math
import struct

import pytest

from tools import anm_inspect as anm
from tools import ktmdl_inspect as mdl

IDENTITY = (1.,0.,0.,0., 0.,1.,0.,0., 0.,0.,1.,0., 0.,0.,0.,1.)


def model_fixture(weight_kind=1):
    components = weight_kind+1 if weight_kind is not None else 0
    index_offset = 12+components*4
    normal_offset,uv_offset = index_offset+4,index_offset+16
    stride = uv_offset+8
    declaration = [0,0,2,16]
    if components:
        declaration += [0,12,weight_kind,32]
    declaration += [0,index_offset,11,33, 0,normal_offset,2,18, 0,uv_offset,1,22]
    b = bytearray(928)
    b[:8] = b"KTMDL\0\0\0"
    struct.pack_into("<II",b,8,2,2)
    b[16:18] = b"\1\0"
    for field,count,offset in [(24,2,192),(32,1,544),(40,1,560),(48,2,656),(136,len(declaration)//4,720),(116,160,768)]:
        struct.pack_into("<Ii",b,field,count,offset)
    struct.pack_into("<I",b,144,len(b))
    for i in range(2):
        start = 192+i*176
        struct.pack_into("<Q",b,start,10+i)
        a,c = list(IDENTITY),list(IDENTITY)
        a[12],c[12] = i*2,-i*2
        struct.pack_into("<16f",b,start+16,*a)
        struct.pack_into("<16f",b,start+80,*c)
        struct.pack_into("<i",b,start+172,-1 if i == 0 else 0)
    struct.pack_into("<2H",b,544,0,1)
    for field,count,relative in [(576,1,96),(584,2,-16),(592,1,128)]:
        struct.pack_into("<Ii",b,field,count,relative)
    struct.pack_into("<iI4Bi",b,656,112,3,1,stride,len(declaration)//4,1,64)
    struct.pack_into("<iI",b,688,768+3*stride-688,3)
    struct.pack_into("<i",b,708,1)
    b[720:720+len(declaration)] = bytes(declaration)
    for i,xyz in enumerate([(0,0,0),(1,0,0),(0,1,0)]):
        start = 768+i*stride
        struct.pack_into("<3f",b,start,*xyz)
        if components:
            struct.pack_into(f"<{components}f",b,start+12,*[.25,.5,0][:components])
        struct.pack_into("<4B3f2f",b,start+index_offset,0,1,0,1 if components == 3 else 255,0,0,1,0,0)
    struct.pack_into("<3H",b,768+3*stride,0,1,2)
    return b


def animation_fixture():
    b = bytearray(288)
    struct.pack_into("<IIHHH",b,0,7,0,30,1,14)
    struct.pack_into("<HHH8H",b,14,6,1,1,*range(8))
    struct.pack_into("<IHH5I",b,64,0xff010001,2,0,0,0,28,48,0)
    struct.pack_into("<4I",b,92,0xff010003,2,16,20)
    struct.pack_into("<BbBb",b,108,0,-1,1,0)
    struct.pack_into("<6I",b,112,0xff010002,0,24,64,128,0)
    for start,curve,node in [(136,0x1c,0),(176,0x1d,0),(240,0x1c,1)]:
        struct.pack_into("<4H2I",b,start,curve,3,3,node,0,16)
        if curve == 0x1c:
            # Original packed bytes; the metadata inspector does not decode them.
            b[start+16:start+34] = bytes(range(18))
        else:
            struct.pack_into("<9f",b,start+16,1,2,3,4,5,6,7,8,9)
    return b


def test_original_bones_palettes_and_unmodified_weights():
    r = mdl.inspect_skeleton(model_fixture())["skeletal_metadata"]
    assert r["roots"] == [0]
    assert [b["parent"] for b in r["bones"]] == [-1,0]
    assert [b["name_id"] for b in r["bones"]] == [10,11]
    assert all(b["a_times_b_identity_error"] == 0 for b in r["bones"])
    assert r["bones"][1]["matrix_a"][12] == 2
    assert r["packets"][0]["palette"] == [0,1]
    assert r["weight_order"] == "unresolved"
    assert r["packets"][0]["raw_influences"] == [
        {"explicit_weights":[.25,.5],"palette_indices":[0,1,0,255]}]*3


@pytest.mark.parametrize("kind", [0,1,2])
def test_explicit_weight_component_counts(kind):
    b = model_fixture(kind)
    struct.pack_into(f"<{kind+1}f",b,780,*[.125,.25,.5][:kind+1])
    r = mdl.inspect_skeleton(b)["skeletal_metadata"]["packets"][0]["raw_influences"][0]
    assert r["explicit_weights"] == [.125,.25,.5][:kind+1]


@pytest.mark.parametrize("offset,fmt,value", [
    (24,"I",0),(24,"I",257),(28,"i",-1),(32,"I",2),(36,"i",600),
    (364,"i",2),(364,"i",0),(540,"i",1),(208,"f",math.nan),
    (544,"H",2),(584,"I",257),(588,"i",-17),(588,"i",0),
    (726,"B",11),(727,"B",33),(730,"B",2),(729,"B",12),
    (780,"f",math.nan),(780,"f",math.inf),(780,"f",-.1),
    (780,"f",1.1),(780,"f",.8),(788,"B",2),
])
def test_reject_invalid_skeletal_metadata(offset,fmt,value):
    b = model_fixture()
    struct.pack_into("<"+fmt,b,offset,value)
    with pytest.raises(ValueError):
        mdl.inspect_skeleton(b)


def test_skeletal_vertex_cap(monkeypatch):
    # Check the cap before decoding influences, using existing geometry metadata.
    original = mdl.inspect_model
    def excessive(b):
        r = original(b); r["vertex_count"] = 65_536; return r
    monkeypatch.setattr(mdl,"inspect_model",excessive)
    with pytest.raises(ValueError,match="vertex limit"):
        mdl.inspect_skeleton(model_fixture())


def test_indices_without_explicit_weights_are_preserved():
    r = mdl.inspect_skeleton(model_fixture(None))["skeletal_metadata"]["packets"][0]
    assert all(v["explicit_weights"] == [] for v in r["raw_influences"])


def test_dense_metadata_and_pairing():
    b = animation_fixture()
    r = anm.inspect_anm(b,core_offset=64,sequence_offset=0,model=model_fixture())
    assert r["parents"] == [-1,0]
    assert r["sample_count"] == 3 and r["last_sample_index"] == 2
    assert r["fps"] == 30 and r["last_sample_seconds"] == 2/30
    assert [(t["node"],t["curve_type"],t["samples"]) for t in r["tracks"]] == [(0,28,3),(0,29,3),(1,28,3)]
    assert r["tracks"][0]["payload_sha256"] == hashlib.sha256(bytes(range(18))).hexdigest()
    assert r["tracks"][1]["translation_min"] == [1,2,3]
    assert r["tracks"][1]["translation_max"] == [7,8,9]
    assert r["sequence"]["tracks"][0]["raw_events"] == [list(range(8))]
    assert r["model_hierarchy_match"] and r["model_sha256"] == hashlib.sha256(model_fixture()).hexdigest()
    core = anm.inspect_anm(b[64:])
    assert core["sequence"] is None and core["fps"] is None and core["last_sample_seconds"] is None


@pytest.mark.parametrize("offset,fmt,value", [
    (64,"I",0),(68,"H",4096),(80,"I",0),(84,"I",28),
    (92,"I",0),(96,"I",129),(100,"I",0),(104,"I",1000),
    (109,"b",2),(109,"b",1),(110,"B",0),(112,"I",0),
    (120,"I",12),(120,"I",1024),(124,"I",24),(124,"I",1000),
    (128,"I",64),(132,"I",1),(136,"H",31),(138,"H",0),
    (140,"H",2),(142,"H",2),(144,"I",1),(148,"I",0),
    (170,"B",1),(192,"f",math.nan),(246,"H",0),
    (8,"H",0),(10,"H",33),(12,"H",1),(14,"H",0),(16,"H",33),(18,"H",257),
])
def test_reject_damaged_dense_animation(offset,fmt,value):
    b = animation_fixture()
    struct.pack_into("<"+fmt,b,offset,value)
    with pytest.raises(ValueError):
        anm.inspect_anm(b,core_offset=64,sequence_offset=0)


@pytest.mark.parametrize("length", [0,63,91,107,111,135,151,169,191,227,255,273])
def test_truncated_animation(length):
    with pytest.raises(ValueError):
        anm.inspect_anm(animation_fixture()[:length],core_offset=64,sequence_offset=0)


def test_bad_offsets_caps_and_mismatched_model(monkeypatch):
    for offset in [-1,289]:
        with pytest.raises(ValueError):
            anm.inspect_anm(animation_fixture(),core_offset=offset)
        with pytest.raises(ValueError):
            anm.inspect_anm(animation_fixture(),core_offset=64,sequence_offset=offset)
    b = model_fixture()
    struct.pack_into("<i",b,540,-1)
    with pytest.raises(ValueError,match="does not match"):
        anm.inspect_anm(animation_fixture(),core_offset=64,model=b)
    monkeypatch.setattr(anm,"MAX_INPUT_SIZE",287)
    with pytest.raises(ValueError,match="limit"):
        anm.inspect_anm(animation_fixture(),core_offset=64)


def test_both_clis_and_output_protection(tmp_path,capsys):
    model,clip = tmp_path/'original.ktmdl',tmp_path/'original.anm'
    model.write_bytes(model_fixture()); clip.write_bytes(animation_fixture())
    output = tmp_path/'model.json'
    args = ['--input',str(model),'--skeleton','--json',str(output)]
    assert mdl.main(args) == 0
    assert json.loads(output.read_text())["skeletal_metadata"]["weight_order"] == 'unresolved'
    before = output.read_bytes()
    assert mdl.main(args) == 1 and output.read_bytes() == before
    output = tmp_path/'animation.json'
    args = ['--input',str(clip),'--core-offset','64','--sequence-offset','0','--model',str(model),'--json',str(output)]
    assert anm.main(args) == 0
    assert json.loads(output.read_text())["model_hierarchy_match"]
    before = output.read_bytes()
    assert anm.main(args) == 1 and output.read_bytes() == before
    assert anm.main(['--input',str(clip)]) == 1
    assert anm.main(['--input',str(tmp_path/'missing')]) == 1
    link = tmp_path/'input-link'; link.symlink_to(clip)
    assert anm.main(['--input',str(link)]) == 1
    output = tmp_path/'dangling-output'; output.symlink_to(tmp_path/'absent')
    assert anm.main(['--input',str(clip),'--json',str(output)]) == 1
    assert anm.main(['--input',str(clip),'--core-offset','64']) == 0
    assert '"fps": null' in capsys.readouterr().out
