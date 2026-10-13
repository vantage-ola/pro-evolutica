"""Original short motion, duplicate geometry and quaternion continuity fixtures."""

import hashlib
import json
import math
import struct
import subprocess
import sys
from pathlib import Path

import pytest

from tools import anm_sequence as sequence
from test_anm_pose import clip_fixture, pack_rotation
from test_skeletal_inspection import model_fixture


def moving_clip():
    clip = clip_fixture()
    struct.pack_into("<9f",clip,192,0,0,0,1,0,0,3,0,0)
    return clip


def measure(model=None,clip=None,**options):
    return sequence.measure_sequence(model if model is not None else model_fixture(),
        clip if clip is not None else moving_clip(),core_offset=64,sequence_offset=0,weight_order="first",**options)


def test_known_root_motion_steps_bounds_and_nonloop_endpoint():
    report = measure()
    assert report["evaluated_samples"] == 3
    assert report["duration_seconds"] == 2/30 and report["fps"] == 30
    assert [f["sample"] for f in report["frames"]] == [0,1,2]
    assert [f["seconds"] for f in report["frames"]] == [0,1/30,2/30]
    assert [f["root_origins"] for f in report["frames"]] == [[[0,0,0]],[[1,0,0]],[[3,0,0]]]
    assert report["frames"][0]["vertex_step_max"] is None
    assert [f["vertex_step_max"]["distance"] for f in report["frames"][1:]] == pytest.approx([1,2])
    assert [f["joint_step_max"]["distance"] for f in report["frames"][1:]] == pytest.approx([1,2])
    assert report["first_last_vertex_distance_max"]["distance"] == pytest.approx(3)
    assert report["first_last_joint_distance_max"] == pytest.approx(3)
    assert report["bounds_min"] == [min(f["bounds_min"][i] for f in report["frames"]) for i in range(3)]
    assert report["bounds_max"] == [max(f["bounds_max"][i] for f in report["frames"]) for i in range(3)]
    assert len({f["position_sha256_f64le"] for f in report["frames"]}) == 3
    assert all(f["triangle_area_min"] > 0 for f in report["frames"])


def test_subrange_first_step_and_single_sample():
    report = measure(start=1,end=2)
    assert report["duration_seconds"] == 1/30
    assert report["frames"][0]["vertex_step_max"] is None
    assert report["frames"][1]["vertex_step_max"]["distance"] == pytest.approx(2)
    report = measure(start=2,end=2)
    assert report["duration_seconds"] == 0
    assert report["first_last_vertex_distance_max"] == {"distance":0.,"packet":None,"vertex":None}
    assert report["frames"][0]["local_rotation_step_degrees_max"] is None


def test_unknown_fps_is_not_invented_and_stationary_positions_have_repeatable_hashes():
    r = sequence.measure_sequence(model_fixture(),clip_fixture(),core_offset=64,weight_order="last")
    assert r["fps"] is None and r["duration_seconds"] is None
    assert all(f["seconds"] is None for f in r["frames"])
    assert len({f["position_sha256_f64le"] for f in r["frames"]}) == 1
    assert all(f["vertex_step_max"]["distance"] == 0 for f in r["frames"][1:])


def test_geometry_digest_uses_ordered_little_endian_float64_positions():
    from tools.anm_pose import evaluate_pose
    _,mesh = evaluate_pose(model_fixture(),moving_clip(),core_offset=64,weight_order="first")
    raw=b''.join(struct.pack("<3d",*v) for p in mesh["packets"] for v in p["positions"])
    assert measure()["frames"][0]["position_sha256_f64le"] == hashlib.sha256(raw).hexdigest()


def test_coincident_bindings_are_separate_from_coincident_source_positions():
    model = model_fixture(0)
    stride = 40
    for i in (1,2):
        struct.pack_into("<3f",model,768+i*stride,0,0,0)
    struct.pack_into("<f",model,768+stride+12,.75)
    report = measure(model=model,clip=clip_fixture())
    assert report["duplicate_position_groups"] == 1
    assert report["identical_binding_groups"] == 1
    assert report["frames"][0]["duplicate_position_anchor_gap_max"] == pytest.approx(math.sqrt(2),abs=1e-4)
    assert report["frames"][0]["identical_binding_anchor_gap_max"] == 0


def test_zero_area_geometry_is_reported_for_review():
    model = model_fixture(None)
    for i in (1,2):
        struct.pack_into("<3f",model,768+i*36,0,0,0)
    report = measure(model=model,clip=clip_fixture())
    assert all(f["zero_area_triangles"] == 1 and f["triangle_area_min"] == 0 for f in report["frames"])
    assert report["identical_binding_groups"] == 1


def test_rotation_distance_treats_quaternion_sign_as_same_orientation():
    assert sequence.rotation_distance([0,0,0,1],[0,0,0,-1]) == 0
    assert sequence.rotation_distance([0,0,0,1],[0,0,1,0]) == 180
    assert sequence.rotation_distance([0,0,0,1],[0,0,math.sqrt(.5),math.sqrt(.5)]) == pytest.approx(90)
    clip = clip_fixture()
    q = [-math.sqrt(.5),0,0,math.sqrt(.5)]
    for i,(value,missing) in enumerate([(q,3),([-v for v in q],0),(q,3)]):
        clip[152+i*6:158+i*6] = pack_rotation(value,missing)
    report = measure(clip=clip)
    # Changing the omitted component quantizes near-zero coordinates differently.
    assert max(f["local_rotation_step_degrees_max"]["degrees"] for f in report["frames"][1:]) < .01
    assert max(f["vertex_step_max"]["distance"] for f in report["frames"][1:]) < 3e-4


def test_known_local_rotation_step_reports_correct_node():
    clip = clip_fixture()
    clip[262:268] = pack_rotation((0,0,0,1))
    report = measure(clip=clip)
    assert report["frames"][1]["local_rotation_step_degrees_max"]["node"] == 1
    assert report["frames"][1]["local_rotation_step_degrees_max"]["degrees"] == pytest.approx(90,abs=.01)


@pytest.mark.parametrize("options",[{"start":-1},{"end":3},{"start":2,"end":1},{"start":True},{"end":1.5}])
def test_invalid_ranges(options):
    with pytest.raises(ValueError,match="range"):
        measure(**options)


@pytest.mark.parametrize("constant,value",[("MAX_SAMPLES",2),("MAX_VERTEX_SAMPLES",8),("MAX_TRIANGLE_SAMPLES",2)])
def test_work_limits_before_any_pose_evaluation(monkeypatch,constant,value):
    monkeypatch.setattr(sequence,constant,value)
    def forbidden(*args,**kwargs):raise AssertionError("pose work started before limit check")
    monkeypatch.setattr(sequence,"evaluate_pose",forbidden)
    with pytest.raises(ValueError,match="limit"):
        measure()


def test_damaged_later_sample_fails_entire_run(tmp_path):
    clip = clip_fixture()
    clip[163] |= 128 # Reserved bit in root sample 1, while sample 0 remains valid.
    with pytest.raises(ValueError,match="reserved bit"):
        measure(clip=clip)
    modelpath,clippath,report = tmp_path/'model',tmp_path/'clip',tmp_path/'report'
    modelpath.write_bytes(model_fixture());clippath.write_bytes(clip)
    assert sequence.main(['--model',str(modelpath),'--animation',str(clippath),
        '--core-offset','64','--weight-order','first','--json',str(report)]) == 1
    assert not report.exists()


def test_cli_reports_and_output_protection(tmp_path,capsys):
    model,clip,report = tmp_path/'model',tmp_path/'clip',tmp_path/'report'
    model.write_bytes(model_fixture());clip.write_bytes(moving_clip())
    args = ['--model',str(model),'--animation',str(clip),'--core-offset','64',
            '--sequence-offset','0','--weight-order','first','--json',str(report)]
    assert sequence.main(args) == 0
    assert json.loads(report.read_text())["evaluated_samples"] == 3
    before = report.read_bytes()
    assert sequence.main(args) == 1 and report.read_bytes() == before
    report.unlink();report.symlink_to(tmp_path/'missing')
    assert sequence.main(args) == 1
    report.unlink();model.unlink();model.symlink_to(clip)
    assert sequence.main(args) == 1 and not report.exists()
    model.unlink()
    assert sequence.main(args) == 1
    model.write_bytes(model_fixture())
    assert sequence.main(args[:-2]) == 0
    assert '"duration_seconds": 0.06666666666666667' in capsys.readouterr().out


def test_script_runs_outside_repository(tmp_path):
    root = Path(__file__).resolve().parents[1]
    model,clip = tmp_path/'model',tmp_path/'clip'
    model.write_bytes(model_fixture());clip.write_bytes(moving_clip())
    result = subprocess.run([sys.executable,str(root/'tools/anm_sequence.py'),
        '--model',str(model),'--animation',str(clip),'--core-offset','64',
        '--weight-order','first'],cwd=tmp_path,capture_output=True,text=True,timeout=10)
    assert result.returncode == 0,result.stderr
    assert json.loads(result.stdout)["evaluated_samples"] == 3
