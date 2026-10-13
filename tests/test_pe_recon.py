"""Static anchors in an original tiny PE, without executing its instructions."""
import json
import struct
import pytest
from conftest import make_pe
from tools import pe_recon as recon


def fixture(tmp_path):
    data = bytearray(make_pe())
    # A real instruction shape and a deliberately unaligned, embedded-data match.
    # Both must remain candidates because the scanner does not decode instructions.
    data[0x210:0x216] = b'\xff\x15'+struct.pack('<I',0x402070)
    data[0x230:0x236] = b'\xff\x25'+struct.pack('<I',0x402070)
    data[0x241:0x247] = b'\xff\x15'+struct.pack('<I',0x402070)
    data[0x600:0x60b] = b'archive.img\0'
    struct.pack_into('<I',data,0x620,0x402200)
    # A matching call shape in a non-executable section must be excluded.
    data[0x630:0x636] = b'\xff\x15'+struct.pack('<I',0x402070)
    path = tmp_path/'anchors.exe';path.write_bytes(data)
    return path


def test_original_import_and_data_anchors(tmp_path):
    path = fixture(tmp_path)
    r = recon.inspect_recon(path,strings=['archive.img'],imports=['ExitProcess'])
    assert r['imports'][0]['iat_va'] == 0x402070
    assert [x['va'] for x in r['imports'][0]['call_byte_candidates']] == [0x401010,0x401041]
    assert r['imports'][0]['jump_byte_candidates'][0]['va'] == 0x401030
    assert 'not instruction-decoded' in r['limitations']
    anchor = r['strings'][0]['occurrences'][0]
    assert (anchor['file_offset'],anchor['rva'],anchor['va']) == (0x600,0x2200,0x402200)
    assert anchor['pointer_byte_candidates'][0]['va'] == 0x402220
    assert r['sections'][0]['executable'] and not r['sections'][1]['executable']
    assert 0 <= r['sections'][0]['entropy_bits_per_byte'] <= 8
    assert recon.inspect_recon(path,strings=['absent'],imports=['ReadFile'])['strings'][0]['occurrences'] == []


def test_limits_and_supported_architecture(tmp_path,monkeypatch):
    path = fixture(tmp_path)
    for values in (['x']*17,['same','same'],['\0'],['x'*257]):
        with pytest.raises(ValueError):recon.inspect_recon(path,strings=values)
    monkeypatch.setattr(recon,'MAX_MATCHES',1)
    with pytest.raises(ValueError,match='occurrence limit'):
        recon.inspect_recon(path,imports=['ExitProcess'])
    monkeypatch.setattr(recon,'MAX_MATCHES',128)
    monkeypatch.setattr(recon,'MAX_SCAN_BYTES',0)
    with pytest.raises(ValueError,match='work limit'):
        recon.inspect_recon(path,imports=['ExitProcess'])
    path.write_bytes(make_pe(bits=64))
    with pytest.raises(ValueError,match='x86 PE32'):recon.inspect_recon(path)
    monkeypatch.setattr(recon,'MAX_BYTES',100)
    with pytest.raises(ValueError,match='64 MiB'):recon.inspect_recon(path)


def test_cli_output_protection_and_special_inputs(tmp_path):
    path = fixture(tmp_path);original = path.read_bytes();out=tmp_path/'report.json'
    args = ['--input',str(path),'--string','archive.img','--import','ExitProcess','--json',str(out)]
    assert recon.main(args) == 0 and json.loads(out.read_text())['imports'][0]['name']=='ExitProcess'
    report = out.read_bytes()
    assert recon.main(args) == 1 and out.read_bytes()==report and path.read_bytes()==original
    out.unlink();out.symlink_to(tmp_path/'missing')
    assert recon.main(args)==1 and not (tmp_path/'missing').exists()
    link=tmp_path/'link';link.symlink_to(path)
    assert recon.main(['--input',str(link)])==1
    import os
    fifo=tmp_path/'fifo';os.mkfifo(fifo)
    assert recon.main(['--input',str(fifo)])==1
