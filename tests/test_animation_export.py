"""Small original model/clip package checks; no game data."""
import struct
import pytest
from tools import animation_export as export
from test_anm_pose import clip_fixture
from test_skeletal_inspection import model_fixture


def test_package_layout_and_resolved_weights():
    clip = clip_fixture()
    struct.pack_into('<3f',clip,204,1,0,0)
    data,report = export.export_animation(model_fixture(0),clip,core_offset=64,sequence_offset=0,weight_order='first')
    assert struct.unpack_from('<8s5I',data) == (b'PEVA0001',2,3,3,3,30)
    assert len(data) == 44+2*132+3*48+3*2+3*2*19
    assert report['duration_seconds'] == 2/30 and report['triangle_count'] == 1
    xyz = struct.unpack_from('<3f4B4d',data,44+2*132)
    assert xyz[3:7] == (0,1,0,0) and xyz[7:] == (.75,.25,0,0)
    samples = 44+2*132+3*48+6
    assert data[samples+6] == 1 and data[samples+19+6] == 0
    assert struct.unpack_from('<3f',data,samples+2*19+7) == (1,0,0)
    assert report['radius'] > 0


def test_limits_and_missing_timing(monkeypatch):
    with pytest.raises(ValueError,match='FPS'):
        export.export_animation(model_fixture(),clip_fixture(),core_offset=64,weight_order='first')
    monkeypatch.setattr(export,'MAX_VERTEX_SAMPLES',1)
    with pytest.raises(ValueError,match='limit'):
        export.export_animation(model_fixture(),clip_fixture(),core_offset=64,sequence_offset=0,weight_order='first')


def test_cli_preserves_existing_outputs(tmp_path):
    model,clip,out = [tmp_path/n for n in ('model','clip','animation.pva')]
    model.write_bytes(model_fixture());clip.write_bytes(clip_fixture())
    args = ['--model',str(model),'--animation',str(clip),'--core-offset','64','--sequence-offset','0',
            '--weight-order','first','--output',str(out)]
    assert export.main(args) == 0
    before = out.read_bytes()
    assert export.main(args) == 1 and out.read_bytes() == before
    out.unlink();out.symlink_to(tmp_path/'missing')
    assert export.main(args) == 1 and not (tmp_path/'missing').exists()
