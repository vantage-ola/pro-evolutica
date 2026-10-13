# Tool verification

Test environment: Python 3.11.9 on macOS ARM64.

## Automated checks

Command:

```bash
.venv/bin/python -m pytest -q
```

Exact final output, also saved in [tools-tests.txt](tools-tests.txt):

```text
........................................................................ [ 17%]
........................................................................ [ 35%]
........................................................................ [ 53%]
........................................................................ [ 71%]
........................................................................ [ 89%]
.........................................                                [100%]
401 passed in 1.38s
```

The test suite generates two-section PE fixtures in pytest temporary directories.
It requires neither game files nor a phone. PE tests check hashes, sizes, addresses,
machine types, named and ordinal imports, delayed imports and IAT fallback. Error
cases include truncated data, invalid offsets/counts, descriptor limits, missing
import terminators, missing files and FIFOs. CLI tests invoke the real scripts,
including from a different working directory, and verify JSON and exit status.

Inventory tests check nested and empty directories, magic-byte length, extensions,
Unicode and quoted CSV paths, symlinks, cycles, special files, unreadable files and
directories, and the entry cap. A race regression replaces an ancestor directory
with a symlink before a queued child is opened; the scan records an error and does
not read the outside file. Both tools refuse to overwrite existing inputs/reports.

The 29 AFS tests generate original padded archives and check entry metadata,
empty slots, streamed hashes, byte-exact extraction, malformed/truncated tables,
invalid offsets/indices, resource caps, and CLI errors. They also check overwrite
and symlink refusal, non-blocking FIFO rejection, detection of input mutation,
and removal of a partial extraction after a truncated read. No proprietary
fixtures or third-party parser packages are needed.

The 33 WESYS tests generate original zlib-wrapped payloads. They check lengths,
hashes, empty decoded data, zero padding, malformed headers, unsupported variants,
size limits, bad checksums, truncated/concatenated streams, and CLI output/errors.
A false-size fixture expands to 2 MiB, but decoding rejects it with less than
512 KiB of traced Python allocations. File tests check overwrite/symlink refusal,
non-blocking FIFO rejection, destination validation, detection of input mutation,
and absence of decoded outputs after format errors.

The 46 texture tests generate original bundle, WE00 and DDS fixtures. They check
selection and exact DDS bytes, all mip sizes, malformed offsets/counts/headers,
dimension limits, unsupported formats/cube maps, zero padding and file/CLI
errors. Golden DXT1 blocks verify four-colour interpolation, one-bit alpha,
selector order and clipping for dimensions smaller than or not divisible by four.
PNG tests verify dimensions, chunk CRCs and decoded RGBA scanlines. No real game
texture is required by the suite.

The 65 KTMDL tests generate an original flat mesh and stream/declaration tables.
They check both observed versions, exact counts and bounds, unsupported types
and primitives, truncated data, table overlap, invalid relative references,
per-stream/aggregate caps, non-finite positions, out-of-range indices, strip
restarts and repeated-index windows. File/CLI tests check stable reads,
symlink/FIFO/directory refusal, JSON output and overwrite protection.

The 47 static-export tests add original identity-bone/material/name fixtures
and known normals, UVs and faces. They check strip parity through degenerate
windows and restarts, exact OBJ indices, area and normal direction, finite
attributes, missing normals/UVs, unsupported bones/skinning/morphs, bad metadata
references/overlap, bounded names and export limits. CLI checks cover distinct
exclusive outputs, format errors producing no OBJ, and direct script execution
from another working directory. The reference reader is not a test dependency.

The 77 skeletal/animation tests generate an original two-bone model and a
three-sample dense clip. They check exact raw matrices, palette indices, one/two/
three explicit weights and the no-weight case, parent cycles/ranges, table and
attribute overlap, weight/vertex caps, malformed offsets and non-finite values.
Animation cases check timing, missing-FPS reporting, structural pairing,
ordered/terminated track tables, duplicate node/curve pairs, sample counts,
unsupported sparse/compressed tracks, translation bounds and raw event records.
Truncation tests require bounded errors rather than unpack exceptions. Both CLI
paths check exclusive reports and malformed/missing/symlink inputs. The real
reference readers remain outside the test suite.

The 41 pose tests check original packed rotations, bone transforms and weighted
geometry. Their cases and the separate real-pose comparison are recorded in
[Pose evaluation](#pose-evaluation).

The 19 sequence tests check known short motion, sampled ranges, quaternion signs,
duplicate bindings, geometry collapse, late corruption, work bounds and CLI/file
errors. The complete real-clip comparison is recorded in
[Dense sequence check](#dense-sequence-check).

```bash
.venv/bin/python -m pip check
```

Result: exit 0, `No broken requirements found.` An independent
`importlib.metadata.version` check found **6/6** installed versions equal to
`requirements-test.txt`.

## Read-only smoke tests

`GAME_ROOT` refers to a local game directory outside the repository.
Game files are not test dependencies and are not in Git.

```bash
.venv/bin/python tools/pe_inspect.py \
  --input "$GAME_ROOT/pes2013.exe" --json private/reports/pes2013-pe.json
.venv/bin/python tools/asset_inventory.py \
  --root "$GAME_ROOT" --json private/reports/pes2013-assets.json \
  --csv private/reports/pes2013-assets.csv
```

Both commands returned 0. The PE report records x86/PE32, 19,916,288 bytes, 19 DLL
descriptors and 330 reported functions. The inventory records 42 files totalling
6,206,731,485 bytes, `complete: true`, zero errors and zero skipped entries.

The same PE CLI inspected `PES2015.exe` and returned 0: x86/PE32,
39,111,472 bytes, 24 DLL descriptors and 623 reported functions. Both PE reports
agree with `file` on architecture and with `shasum -a 256` on hashes. Fingerprints
and the PES 2013 parser warning are recorded in [../STATUS.md](../STATUS.md).
Raw JSON/CSV reports remain in ignored `private/reports/`.

## AFS inspection and extraction

`inspect_afs()` inspected all 19 local PES 2013 `.img` archives successfully:
55,593 slots, including 1,273 empty. Their full SHA-256 fingerprints and entry
reports were saved under ignored `private/reports/`.

The API and CLI each extracted `dt10.img` entry 1:

```bash
.venv/bin/python tools/afs_inspect.py \
  --input "$GAME_ROOT/img/dt10.img" --entry 1 \
  --output private/extracted/dt10-entry-00001-cli.bin \
  --json private/reports/dt10-extraction-cli.json
shasum -a 256 "$GAME_ROOT/img/dt10.img" \
  private/extracted/dt10-entry-00001-cli.bin
```

The CLI returned 0. An independent seek/read at offset 2,048 for 1,840 bytes
matched the extracted bytes exactly. `shasum` agreed with the archive and output
hashes in [the research record](../research/pes2013.md). `git check-ignore`
confirmed both the raw extraction and report are excluded from Git.

## WESYS decoding

`inspect_wesys()` decoded all ten non-empty `dt10.img` entries: 140,960 bytes in
total. Each output matched a separate one-shot `zlib.decompress()` of the declared
compressed range, using the same underlying zlib library. Each compressed stream
reached its end marker with no unused or unconsumed bytes, a valid checksum and
the exact declared decoded length. Padding was zero-filled, 2–10 bytes per entry.

```bash
.venv/bin/python tools/wesys_unpack.py \
  --input private/extracted/dt10-entry-00001.bin \
  --output private/extracted/dt10-entry-00001-decoded.bin \
  --json private/reports/dt10-entry-00001-wesys.json
shasum -a 256 private/extracted/dt10-entry-00001-decoded.bin
```

The CLI returned 0 and produced 28,272 bytes. `shasum` agreed with
`2cf527971ca3b7222c69dd0d3681e56a79fbe319763f8070288274fcb42e5e56`.
`git check-ignore` confirmed the decoded file and report remain excluded.
The runtime used Python's bundled zlib 1.2.12; no new dependency was installed.

## Real texture extraction and preview

The existing AFS and WESYS CLIs extracted `dt08.img` entry 2 and decoded it into
a 43,888-byte bundle. The texture CLI then selected bundle index 0:

```bash
.venv/bin/python tools/pes_texture.py \
  --input private/extracted/dt08-entry-00002-decoded.bin \
  --output private/extracted/dt08-entry-00002.dds \
  --png private/extracted/dt08-entry-00002.png \
  --json private/reports/dt08-entry-00002-texture.json
shasum -a 256 private/extracted/dt08-entry-00002.dds \
  private/extracted/dt08-entry-00002.png
/usr/bin/sips -s format png private/extracted/dt08-entry-00002.dds \
  --out private/extracted/dt08-entry-00002-reference.png
```

All commands returned 0. The DDS is 256 × 256 DXT1, nine mip levels, 43,832 bytes;
it exactly matches the source range at offset 48. The PNG is 17,631 bytes.
`shasum` agrees with both hashes in [the research record](../research/pes2013.md).

The generated PNG was visually inspected. An independent macOS DDS conversion
also succeeded. After checking PNG CRCs and reversing their scanline filters,
RGBA comparison found zero alpha differences and a maximum RGB difference of
1/255; 15,547 of 262,144 channels differ. This is a close pixel comparison, not
an exact match to the independent decoder. The DDS extraction itself is exact.
The reference check is separate from pytest and is specific to this local sample.
`git check-ignore` confirms the DDS, PNG and report are excluded from Git.

## KTMDL geometry metadata

The AFS and WESYS CLIs independently repeated the spatial sample's extraction:

```bash
.venv/bin/python tools/afs_inspect.py \
  --input "$GAME_ROOT/img/dt0b.img" --entry 17 \
  --output private/extracted/dt0b-entry-00017-model-cli.bin \
  --json private/reports/dt0b-entry-00017-model-afs-cli.json
.venv/bin/python tools/wesys_unpack.py \
  --input private/extracted/dt0b-entry-00017-model-cli.bin \
  --output private/extracted/dt0b-entry-00017-model-cli.decoded.bin \
  --json private/reports/dt0b-entry-00017-model-wesys-cli.json
```

Both returned 0. `cmp` returned 0 against the exploratory extraction/decoding.
The standalone models were taken from bounds-checked bundle entry 0: offset 32,
size 1,120 for entry 35; offset 64, size 48,384 for entry 17. Independent archive
seek/read comparisons and `shasum` checks agree with the
[research record](../research/pes2013.md#selected-ktmdl-model-samples).

```bash
.venv/bin/python tools/ktmdl_inspect.py \
  --input private/extracted/dt0b-entry-00017.ktmdl \
  --json private/reports/dt0b-entry-00017-model-cli.json
```

The CLI returned 0 and agrees with a separate direct header/stream read on the
682 vertices and 1,748 indices. The small sample reports four of each. These
comparisons use the same inferred layout and are not an independent geometry
decoder. Both models pass the new inspector's bounded ranges, finite-position
and index checks. `git check-ignore` confirms model files and full reports stay
private. The independent comparison below extends these metadata checks.

## Static export and independent comparison

```bash
.venv/bin/python tools/ktmdl_export.py \
  --input private/extracted/dt0b-entry-00017.ktmdl \
  --obj private/extracted/ball.obj --json private/reports/ball-static.json
.venv/bin/python tools/pes_texture.py \
  --input private/extracted/dt0b-entry-00017-model-cli.decoded.bin --index 1 \
  --output private/extracted/ball-colour.dds \
  --png private/extracted/ball-colour.png --json private/reports/ball-colour.json
shasum -a 256 private/extracted/ball.obj private/extracted/ball-colour.dds \
  private/extracted/ball-colour.png
/usr/bin/sips -s format png private/extracted/ball-colour.dds \
  --out private/extracted/ball-colour-reference.png
```

The CLIs returned 0; independent hashes agree with
[the research record](../research/pes2013.md#static-ball-geometry-and-textures).
The reviewed standalone reference at commit
`0a0284fc9127beca45397ba7009883af2f2be6fd` was run without modification on the
same model. All six vertex attributes, raw indices and ordered triangles agree
exactly. Identity bone matrices and all three name/ID/mode/UV/parameter bindings
also agree. The private report records 16 passing comparisons, including OBJ
float32 round-trips and face indices. It is separate from the original-fixture
pytest suite.

To repeat the geometry comparison after reviewing the pinned reference source:

```python
import hashlib
import importlib.util
from pathlib import Path
from tools.ktmdl_export import decode_static_model

reference = Path("private/reference/ktmdl/ktmdl.py")
assert hashlib.sha256(reference.read_bytes()).hexdigest() == \
    "6dc2e924e9df136d72476343a0fd27feccac099fcdeb317ec9bcc49522d8ca63"
spec = importlib.util.spec_from_file_location("reference_ktmdl", reference)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
data = Path("private/extracted/dt0b-entry-00017.ktmdl").read_bytes()
report, mesh = decode_static_model(data)
packet = module.parse_bytes(data)["packets"][0]
for semantic, name in {16:"POSITION", 18:"NORMAL", 22:"TEXCOORD0",
                       23:"TEXCOORD1", 30:"TANGENT", 31:"BINORMAL"}.items():
    assert mesh["attributes"][semantic] == [v[name] for v in packet["vertices"]]
assert mesh["indices"] == packet["indices"]
assert mesh["triangles"] == packet["triangles"]
print("682 vertices, 1748 indices, 1144 triangles: exact agreement")
```

The exported colour PNG and independent macOS DDS preview were decoded with
the existing native PNG reader: 512 × 512, 1,048,576 RGBA bytes each. Maximum
RGB error is 1/255, alpha matches exactly, and 13,416 channels differ. All assets,
reference source and full reports remain ignored. Texture ID-to-bundle selection
is a sample-specific inference supported by visual roles; final rendered UV
alignment remains a device check. The flat model's debug-name layout failed in
the pinned reference and is not claimed as a successful full decode.

## Skeletal and animation metadata

With `PES2013_ROOT` pointing to the local game directory, these extraction CLIs
returned 0 and agreed byte-for-byte with independent archive seek/read results:

```bash
.venv/bin/python tools/afs_inspect.py --input "$PES2013_ROOT/img/dt0c.img" \
  --entry 1 --output private/extracted/skinned-entry.bin --json private/reports/skinned-afs.json
.venv/bin/python tools/wesys_unpack.py --input private/extracted/skinned-entry.bin \
  --output private/extracted/skinned-bundle.bin --json private/reports/skinned-wesys.json
.venv/bin/python tools/afs_inspect.py --input "$PES2013_ROOT/img/dt0e.img" \
  --entry 1 --output private/extracted/animation-entry.bin --json private/reports/animation-afs.json
```

The selected ranges can be reproduced with this sample-specific extraction.
It validates the stored wrapper and all bundle ranges, checks the selected
chunk hashes and writes new files. It does not extend the zlib decoder to a
new wrapper version or assume other files share this layout.

```python
import hashlib, struct
from pathlib import Path

raw = Path("private/extracted/animation-entry.bin").read_bytes()
assert raw[:8] == b"\0\1\0WESYS"
assert struct.unpack_from("<II", raw, 8) == (len(raw)-16, 0)
for data, index, name, expected in [
    (Path("private/extracted/skinned-bundle.bin").read_bytes(), 2, "reduced-model.ktmdl",
     "40a19401fed28410c92626f2596d8f9a7abfe5475e5ffd2038e6b325af4716b1"),
    (raw[16:], 0, "selected.anm",
     "070901d5e57773790db7ead1e0c6e209cb9fefdee24380404fee80f2dbdc9b9c"),
]:
    count, table = struct.unpack_from("<II", data)
    assert table == 8 and 1 <= count <= 4096
    entries = list(struct.iter_unpack("<III", data[8:8+count*12]))
    assert len(entries) == count
    assert all((not size or off >= 8+count*12) and off+size <= len(data)
               for off, size, unknown in entries)
    offset, size, unknown = entries[index]
    chunk = data[offset:offset+size]
    assert hashlib.sha256(chunk).hexdigest() == expected
    with (Path("private/extracted") / name).open("xb") as output:
        output.write(chunk)
```

The [skeletal/ANM CLI commands](../TOOLS.md#inspect-skeletal-metadata-and-a-dense-animation)
return 0 on these chunks. Full archive SHA-256 checks agree with the previous
inventory. Both standalone chunk hashes and sizes match the
[research record](../research/pes2013.md#skeleton-and-dense-animation-samples).
The shared palette count field required checking its actual byte region rather
than treating it as the index count.

Independent pinned-reader comparisons give exact agreement on all 19 bone
identifiers/parents/matrix bytes, 34 palettes and 6,736 raw vertex influences.
Every rotation payload agrees with the reference's packed bytes; repacking
its translations to float32 agrees with our translation payload hash. All 20
track node/type/mode/count records, 30 FPS sequence timing and six raw event
records agree. Files and the comparison script/output stay in ignored
`private/research/animation/`; no third-party package was installed.

The Android app remains the measured static ball viewer. This verifies
skeletal and dense-clip metadata for one local pair, not animated poses,
residual-weight ordering or playback. Sparse and compressed translation curves
remain unsupported. The next check is an original pose fixture followed by a
real pose comparison before adding deformation. That next check is recorded below.

## Pose evaluation

```bash
.venv/bin/python -m pytest -q
.venv/bin/python tools/anm_pose.py \
  --model private/extracted/reduced-model.ktmdl \
  --animation private/extracted/selected.anm --core-offset 960 --sequence-offset 816 \
  --sample 45 --weight-order first \
  --obj private/extracted/pose45.obj --json private/reports/pose45.json
shasum -a 256 private/extracted/pose45.obj
```

The full suite returns 0: **401 passed in 1.38s**, Python 3.11.9. Exact output is
in [tools-tests.txt](tools-tests.txt). `tests/test_anm_pose.py` adds 41 original
tests. They cover a literal packing example, all four omitted quaternion
components, signed components, known axis rotations, bind recovery for all
supported weight widths/orders, translated joints, absolute local rotations,
parent/root propagation, unordered bone tables and palette mapping. Invalid
packing, non-rigid/reflected/non-affine/wrong-inverse matrices, morphs, incomplete
rotations, missing influences, sample/index limits, mismatched hierarchies,
input symlinks and output protection are checked. Test inputs contain no game data.

The pose CLI returns 0 for the selected inputs and sample 45. Its report and
OBJ agree exactly with the separately checked evaluator output. Independent
`shasum` agrees with the recorded OBJ hash:
`3ebe1247b03f7d82ca3408aafb7cc6d52c5331d9ea803f9ef91e40d4fc173794`.

Private comparisons check all 1,729 packed rotations against the pinned ANM
reader, with maximum component error 0. A separate vector/quaternion calculation
over the independent readers' model/clip data agrees on all 6,736 vertices and
19 joint origins in sample 45 within 1.056815e-6 source units, for both explicit
weight choices. Triangle lists agree exactly. This calculation uses exact inverse
parent rest matrices rather than stored B for local translations. Blender itself
was not available and was not installed; the comparison is numerical, based on
the reference source's conventions, not its running add-on or an in-game capture.

A CPU render of the reference positions was visually inspected: the bind model
is coherent, residual-first gives a coherent raised-arm pose, and residual-last
badly stretches the geometry. Exact duplicate-position groups also separate
much further under last (maximum 0.712780 versus 0.011466). This supports first
for this sample; it does not establish every model's convention or identical
weights on separate surfaces. Bind roundtrip distance is at most 2.047639e-6
across both choices. See [the detailed interpretation](../research/pes2013.md#one-evaluated-pose).

Inputs, both OBJs, reference values, source copies, comparison code/output and
the render remain ignored under `private/`. No dependency was added and no
game asset or third-party source enters Git/APK. The README and Android app
are unchanged. The sampled sequence is checked below. General animation support,
event meaning, game materials and Android playback remain unverified.

## Dense sequence check

```bash
.venv/bin/python -m pytest -q
.venv/bin/python tools/anm_sequence.py \
  --model private/extracted/reduced-model.ktmdl \
  --animation private/extracted/selected.anm --core-offset 960 --sequence-offset 816 \
  --weight-order first --json private/reports/sequence-first.json
```

Both commands return 0. The full suite is **401 passed in 1.38s**, Python 3.11.9;
[tools-tests.txt](tools-tests.txt) contains the exact output. Nineteen new tests
use the original two-bone/three-sample fixtures. They check known root movement
and endpoint distance, stationary hashes, inclusive subranges, a single sample,
unknown FPS, coincident source points with same/different bindings, zero-area
faces and local rotation changes. A quaternion sign change counts as the same
orientation, allowing measured packing drift. Later reserved-bit corruption
fails the whole run without writing a report. Work caps are checked before any
pose evaluation. CLI tests check missing/symlink inputs, exclusive reports,
stdout and direct execution outside the repository.

The real report covers all 91 samples, 0–90, at 30 FPS, with first-to-last duration
3 seconds. Each frame has 6,736 vertices and 10,079 faces. A separate private
quaternion/vector calculation over the pinned readers' values checks every frame:
612,976 vertices and 1,729 joint origins, maximum position error 1.233889e-6
source units. Triangle lists match exactly. Adjacent maximum vertex/joint movement
agrees within 1.940458e-7/2.263509e-7. A separate evaluator run reproduces all
per-sample position hashes. Full outputs and comparison code are in ignored
`private/research/sequence/`; no reference source is a test dependency.

There are no collapsed faces in 917,189 sampled triangles. The 1,722
identical-position/binding groups remain coincident within 2.237726e-16.
Maximum adjacent vertex movement is 0.301164 from sample 3 to 4; the maximum
local angular step is 33.379384° from 5 to 6. Those faster opening movements
agree with the reference. First-to-last vertex/joint gaps are 1.149206/0.899220;
the clip must not be assumed to loop. Sampled bounds and other gaps are in
[the detailed record](../research/pes2013.md#full-dense-sequence).

A fixed-camera CPU reference render was generated for all 91 samples. A sheet
of samples 0, 4, 6, 15, 30, 45, 60, 75 and 90 was visually inspected: the body
remains coherent through the opening movement and raised-arm poses. The existing
local FFmpeg encoded a 320 × 512 GIF without automatic repeat; FFprobe confirms
91 frames and 3.03 seconds including the final frame's display time. No new
dependency was installed. This preview uses flat shading and sampled poses,
not game materials or native playback. Render code, frames, sheet and GIF stay
private. Neither Blender nor the game animation runtime was used.

The sequence check supports implementing this subset in a portable native
module. Interpolation, automatic loops, events, gameplay role and device
playback still need separate evidence. README and APK remain unchanged.

## Review and limits

Reports preserve parser warnings; an import list with warnings requires further
review. These checks verify diagnostics, AFS extraction, the observed WESYS/zlib
wrapper and the supported flat DXT1 texture workflow on macOS ARM64. They do not
verify script bytecode, other texture formats, full model rendering, REA/Ghidra,
native rendering or device connectivity. The KTMDL checks additionally verify
the supported geometry metadata on two local models and exact static ball
geometry/reference agreement on entry 17. The skeletal/ANM checks verify the
stated metadata subset and one evaluated pose for the selected 19-bone pair.
They do not verify general skeletons, animation playback,
proprietary material shaders or Android model rendering. Details and hashes are in
[the model research record](../research/pes2013.md#selected-ktmdl-model-samples).
The manual Winlator report is recorded separately in
[the phone baseline](../benchmarks/pes2013-baseline.md); sustained performance
has not been measured.

## Executable reconnaissance

```bash
.venv/bin/python -m pytest -q
.venv/bin/python tools/pe_recon.py --input '/path/to/pes2013.exe' \
  --string dt0b.img --string dt0c.img --string dt0e.img \
  --json private/reports/engine-anchors.json
```

The suite passes: **407 tests in 1.30 seconds**, Python 3.11.9. Three new tests
use the existing original PE fixture. They check IAT/ASCII/pointer addresses,
call/jump-shaped byte candidates, exclusion of non-executable sections, missing
selectors, scan/count/file limits, unsupported CPUs, special files and output
protection. Deliberately embedded byte patterns remain labelled candidates;
this scanner does not manufacture instruction boundaries or function names.
The original PE CLI and its tests still pass after exposing validation of an
in-memory snapshot for reuse. No package is installed.

The real-file command returns 0. It scans 353,863,680 cumulative section bytes
within the 1 GiB work cap. Three archive-name VAs and their literal pointer
bytes agree with an independent `struct` parse of the DOS/PE section headers.
An independent SHA-256 agrees before/after inspection. Adjacent data records
pair archive and directory pointers; fields/consumer behaviour remain unknown.
The seven selected import slots yield no direct call/jump byte candidates.
Section entropy suggests transformed code but does not prove its mechanism.
Apple LLVM's unchanged-input debug-directory error and the unverified
Ghidra/REA setup are recorded in
[the engine findings](../research/pes2013-engine.md). No original game function
has been recovered, executed or ported. Full reports remain private.

The same command against the supplied Winlator installation copy also returns
0. Both executable files have the same size and SHA-256; an independent `cmp`
returns 0 and both reconnaissance JSON reports compare equal. The copy does not
resolve the static-code limitation. Only `pes2013.exe` was compared; no claim is
made about equality of the full game directories or original-build provenance.
