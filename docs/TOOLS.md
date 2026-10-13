# Local inspection tools

Use Python 3.11 or newer. The tools read local files; none executes a game.
Keep game directories outside the repository and reports under `private/`.

## Setup and tests

```bash
python3.11 -m venv .venv
.venv/bin/python -m pip install -r requirements-test.txt
.venv/bin/python -m pytest -q
.venv/bin/python -m pip check
```

`pyproject.toml` declares the project and direct dependencies.
`requirements-test.txt` pins the full environment used for these tests.
The installed package metadata identifies `pefile` and `pytest` as MIT licensed.

## Inspect an executable

Set `GAME_ROOT` to your local game directory. Pass the executable path rather
than assuming its name.

```bash
GAME_ROOT='/path/to/Pro Evolution Soccer 2013'
mkdir -p private/reports
.venv/bin/python tools/pe_inspect.py \
  --input "$GAME_ROOT/pes2013.exe" \
  --json private/reports/pes2013-pe.json
```

Omit `--json` to print the same report on stdout. Existing report files, including
symlinks, are refused. Choose a new report name for subsequent runs. The parent
output directory must exist.

The version 1 report includes:

- `file_name`, `file_size` in bytes, and SHA-256 of the exact bytes inspected.
- `architecture` from the COFF machine field, the raw `machine` value, `bits` and
  `pe_format` from the optional header. Unknown machines remain `unknown`.
- `image_base` and `entry_point.rva` / `entry_point.va` as integer addresses.
- `sections` with names, virtual addresses/sizes, and raw file offsets/sizes.
- `imports` with DLL, `standard` or `delay` kind, and function name or ordinal.
- `warnings` from `pefile`, preserved for review. Warnings can mean that import
  parsing was incomplete. An empty list of imports does not prove that the game
  loads no libraries at runtime.

The inspector reports static tables, including delayed imports. Libraries loaded
through runtime calls require later analysis. It checks header and section bounds,
limits input to 256 MiB, sections to 96, and each import descriptor list to 1,024.
`pefile` also bounds import-symbol parsing. An unsupported or damaged file may be
rejected; keep its error and fingerprint for follow-up. It does not allocate the
virtual image, decompress assets, or invoke Windows code.

The header and import interpretation follows the
[Microsoft PE specification](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format)
and the [pefile source](https://github.com/erocarrera/pefile/tree/v2024.8.26).

## Inventory a game directory

```bash
.venv/bin/python tools/asset_inventory.py \
  --root "$GAME_ROOT" \
  --json private/reports/pes2013-assets.json \
  --csv private/reports/pes2013-assets.csv
```

Either output flag works alone. Omit both to print JSON on stdout. Existing output
files are refused; JSON and CSV must use different paths. Keep reports outside the
input directory so future scans do not count old reports as assets.

The JSON report contains sorted, root-relative POSIX paths under `files`, with
`size_bytes`, a lowercase final `extension` (empty for extensionless files), and
`magic_hex` containing up to the first 16 bytes. It also includes `file_count`,
`total_size_bytes`, `skipped`, `errors`, and `complete`. The root name is recorded,
without an absolute root path. The CSV has one row per file, skipped entry, or
error; `status` distinguishes `ok`, `skipped`, and `error`.

Magic bytes and extensions are observations. The inventory does not claim to
identify or support a container format. Symlinks and special files are skipped;
directory and file opens stay anchored to the root and refuse symlinks. A scan
stops at 100,000 visited entries to bound memory. Split larger collections into
separate scans. Scan a directory while its contents are stable; this is not a
transactional filesystem snapshot.

Both commands return 0 on success and 1 on an input, output, or processing error.
Argument syntax errors return 2. An inventory with read errors saves the available
rows with `complete: false`, then returns 1. Review those errors before treating a
report as exhaustive. If writing one of two requested outputs fails, an earlier
output may already exist; check the command's exit status.

## Inspect an AFS container and extract one entry

The locally inspected PES 2013 `.img` files have AFS headers. This reader supports
their flat entry tables; it does not decode the contained assets.

```bash
.venv/bin/python tools/afs_inspect.py \
  --input "$GAME_ROOT/img/dt10.img" --json private/reports/dt10-afs.json

mkdir -p private/extracted
.venv/bin/python tools/afs_inspect.py \
  --input "$GAME_ROOT/img/dt10.img" \
  --entry 1 --output private/extracted/dt10-entry-00001.bin \
  --json private/reports/dt10-extraction.json
```

Indices start at zero. `dt10.img` slot 0 is empty in the inspected copy; slot 1
contains 1,840 bytes. Choose entries from the report for other copies. Empty
slots cannot be extracted. `--entry` and `--output` must be supplied together.

Without `--json`, the report goes to stdout. It contains the archive's name,
size and SHA-256, entry/empty counts, and each entry's index, offset, size and
first 16 bytes in hex. Extraction adds the output name, index, size and SHA-256.
No filename stored inside the archive is used to create an output path.

The reader requires `AFS\0`, then a little-endian unsigned 32-bit count and pairs
of 32-bit offsets/sizes. It checks table and payload bounds, rejects non-empty
entries overlapping the header/table, and preserves empty slots. Optional name
tables and unknown trailing metadata are ignored. It streams the full archive
hash and selected extraction in 1 MiB chunks. Limits: 100,000 slots, 8 GiB input,
256 MiB per extraction. Overlapping payloads are allowed; these checks do not
prove that their contents are valid assets.

Use stable regular files. The final input path cannot be a symlink. Size and
timestamp checks detect common concurrent changes, but do not provide a file
snapshot. If a command fails, discard any extraction from that run; an output
may remain if a later check or report write fails. Output files are created
exclusively, and existing files/symlinks are refused. Parent directories must
exist. Exit statuses are 0 for success, 1 for file/format errors and 2 for argument
errors. Keep extracted bytes under ignored local storage.

This is an independent standard-library implementation. No third-party parser
was installed or copied. Format references and real-sample checks are in
[research/pes2013.md](research/pes2013.md).

## Decode an extracted WESYS entry

```bash
.venv/bin/python tools/wesys_unpack.py \
  --input private/extracted/dt10-entry-00001.bin \
  --output private/extracted/dt10-entry-00001-decoded.bin \
  --json private/reports/dt10-entry-00001-wesys.json
```

Omit `--output` to inspect and validate without saving decoded bytes. Omit `--json`
to print the report on stdout. Existing destinations are refused; parent
directories must exist. Exit statuses are 0 for success, 1 for file/format errors
and 2 for argument errors. Keep decoded content under ignored `private/`.

This reader supports the observed `00 01 01 WESYS` header with little-endian
compressed/decoded lengths and one complete zlib stream. It accepts up to 15 zero
padding bytes after the declared compressed range, as seen in the local AFS
entries. Other header variants, non-zero trailing data, concatenated streams,
truncated data, checksum errors and incorrect decoded lengths are rejected.

Input and decoded output are each capped at 64 MiB. Decompression stops at the
declared decoded length plus one byte, so a false small size cannot trigger
unbounded expansion. The input is read into a bounded memory buffer before
decoding. Use stable regular files; the final input path cannot be a symlink.
Size/timestamp checks detect common concurrent changes. File read errors and
format errors create no decoded output; a later write/report failure may leave
an output that should be discarded.

Reports contain input and decoded SHA-256 hashes, header bytes, compressed and
decoded sizes, zero-padding length and the first 32 decoded bytes in hex. The
decoder does not classify, interpret or execute the contents. It uses Python's
standard-library `zlib`; no new package is required.

## Extract a verified DXT1 texture

The inspected `dt08.img` AFS entry 2 decodes into a bundle containing one WE00
texture. AFS entry indices and decoded bundle indices are separate: the texture
below is **AFS entry 2, bundle index 0**.

```bash
.venv/bin/python tools/afs_inspect.py \
  --input "$GAME_ROOT/img/dt08.img" --entry 2 \
  --output private/extracted/dt08-entry-00002.bin
.venv/bin/python tools/wesys_unpack.py \
  --input private/extracted/dt08-entry-00002.bin \
  --output private/extracted/dt08-entry-00002-decoded.bin
.venv/bin/python tools/pes_texture.py \
  --input private/extracted/dt08-entry-00002-decoded.bin --index 0 \
  --output private/extracted/dt08-entry-00002.dds \
  --png private/extracted/dt08-entry-00002.png \
  --json private/reports/dt08-entry-00002-texture.json
```

Create the ignored output directories first. Use new names if outputs already
exist. Omit `--output` and `--png` for metadata only; omit `--json` for stdout.
The reader preserves the DDS header and all mip levels byte-for-byte. PNG output
decodes only the largest level, with one-bit alpha transparency when present.
Reports include container/entry metadata, the opaque WE00 header, texture
dimensions/format, each mip's offset/size, and input/DDS/PNG hashes.

Supported subset: a little-endian bundle count/table at offset 8, 12-byte records,
and a selected WE00 entry with a 16-byte wrapper around a flat 2D DXT1 DDS.
Unknown record/header fields are preserved, not interpreted. All bundle entries
are bounds-checked; DDS header sizes, dimensions, mip count and exact payload
length are checked. Up to 15 zero bytes after the WE00 DDS range are accepted.
Cube maps, volume textures, other DDS formats and unknown layouts are rejected.

Limits: 64 MiB input, 4,096 bundle entries, 4,096 pixels per dimension and
4,194,304 pixels total. The parser and PNG preview use only the standard library.
Stable regular inputs, final-path symlink refusal, exclusive outputs and exit
statuses follow the WESYS tool. A later write failure may leave earlier outputs;
discard outputs from a failed run. No texture is included in the repository.

## Inspect an extracted KTMDL model

```bash
.venv/bin/python tools/ktmdl_inspect.py \
  --input private/extracted/dt0b-entry-00017.ktmdl \
  --json private/reports/model.json
```

Supply a standalone model extracted from your own asset bundle. The inspector
does not scan AFS files or extract bundle entries; the research record identifies
the checked model ranges. Omit `--json` for stdout; output files must be new.

Supported subset: little-endian KTMDL 2.1/2.2, triangle-strip packets, one vertex
and one 16-bit index stream per packet, and the observed vertex element types
(one/two/three float32 values or four bytes). Position must be three float32 values.
Other attributes retain numeric type/semantic metadata, without decoding values.

Reports include input SHA-256, version, packet/vertex/index counts, declarations,
position bounds, restart markers and triangle windows without repeated indices.
That count does not test triangle area or prove a correct winding order.
Materials, UV values and animation are not parsed. Bone/weight metadata is
available with the optional `--skeleton` flag below.

Limits: 64 MiB input, 4,096 records per geometry table, 1,000,000 vertices and
2,000,000 indices per stream and in aggregate. Geometry tables must be disjoint;
all stream/declaration references stay within their declared regions. The reader
rejects non-finite positions, out-of-range indices, final-path symlinks, special
files, changing inputs and existing report paths. It uses only the standard library.

## Export a supported static KTMDL packet

```bash
.venv/bin/python tools/ktmdl_export.py \
  --input private/extracted/dt0b-entry-00017.ktmdl \
  --obj private/extracted/ball.obj \
  --json private/reports/ball-static.json
```

This builds on the inspector's checked stream offsets. It supports exactly one
triangle-strip packet with float3 positions/normals and float2 UV0, no bones or
one identity root bone, and no skinning, morphs or extra records. It converts
strips to triangle lists: repeated-index windows consume parity, while `65535`
restart markers reset it. It rejects zero-area faces, zero-length normals and
non-finite attributes. Other supported vertex attributes are checked but are
not written into OBJ. Limits: 65,535 vertices and 200,000 source indices.

OBJ uses one-based `v/vt/vn` indices and nine significant digits to preserve
float32 values. Source XYZ, units and UVs stay unchanged. No texture or MTL is
written, and no game shader is recreated. A viewer must handle the image's row
origin explicitly; the export does not decide which source axis represents up.

The JSON adds triangle areas, winding agreement with normals, normal lengths,
UV0 bounds, identity-bone status and packet texture references. Texture names,
IDs, modes, UV channels and parameters are metadata, not automatic bundle
selection. Known metadata tables and debug-name ranges are bounds-checked and
must not overlap geometry. Unknown layouts can be refused even when the earlier
geometry-only inspector accepts them.

Both output paths must be new and distinct. Input/file errors follow the
inspector's stable regular-file checks. A format error creates no output;
a later write error may leave an earlier output, so discard outputs from a
failed run. Keep converted game models under ignored `private/`.


## Inspect skeletal metadata and a dense animation

```bash
.venv/bin/python tools/ktmdl_inspect.py \
  --input private/extracted/reduced-model.ktmdl --skeleton \
  --json private/reports/reduced-model.json
.venv/bin/python tools/anm_inspect.py \
  --input private/extracted/selected.anm --core-offset 960 --sequence-offset 816 \
  --model private/extracted/reduced-model.ktmdl --json private/reports/selected-anm.json
```

The inputs are standalone chunks from the checked local samples; see
[the extraction ranges](research/pes2013.md#skeleton-and-dense-animation-samples).
Core/sequence offsets are explicit and specific to the selected ANM. A core at
file offset zero needs no `--core-offset`. Omit `--sequence-offset` when timing
is unknown: FPS stays `null`, with no assumed default. Omit `--model` for clip
metadata only. All reports must use new paths; omit `--json` for stdout.

`--skeleton` checks acyclic parent links, finite matrices, shared palette ranges
and raw vertex influences. It preserves both 4 × 4 matrix arrays and their
product's maximum error from identity. Explicit float weights and all four byte
indices are retained without normalization or a choice of leftover-weight slot.
The first explicit-weight-count-plus-one indices must fit the packet palette;
unused bytes stay uninterpreted. Weights must be finite, within 0–1 and sum to
at most 1 plus 2e-6 for float rounding. Missing weight attributes are preserved
as an empty explicit-weight list. No deformation is performed.

Supported skeletal limits: 256 bones, 65,535 vertices, the geometry inspector's
existing record/index caps, and the observed shared palette region before the
packet table. The header's count field is 1 in the checked skinned sample even
though its palette holds 19 indices. Its general count meaning is unresolved;
other palette layouts are refused. Known metadata tables must be disjoint.

The ANM inspector checks the observed little-endian `FF010001` core,
`FF010003` hierarchy and `FF010002` track table. Supported curves are dense mode
3, six-byte packed rotation payloads (`0x1c`) and float3 translations (`0x1d`).
It checks unique node/curve pairs, sample counts, ordered offsets, payload spans,
finite translations and up to 15 zero padding bytes. Track payload hashes permit
byte comparisons; packed quaternions are not decoded. Sparse curves, compressed
translation and unknown layouts are refused.

Limits: 64 MiB input, 128 uniquely indexed hierarchy nodes, 256 table entries,
4,096 samples per track, 32 sequence tracks/channels and 256 event records per
sequence track. Sequence headers report FPS only after their channel/event ranges
pass checks. Event records retain eight raw uint16 fields with unknown meaning.
The optional model check requires exact ordered parent-link agreement; that is
a structural pairing check, not proof of correct animated deformation. Input
files use the existing stable regular-file and final-path symlink checks. No new
dependency is needed. Keep source chunks and full reports under ignored storage.

## Evaluate one dense animation pose

```bash
.venv/bin/python tools/anm_pose.py \
  --model private/extracted/reduced-model.ktmdl \
  --animation private/extracted/selected.anm --core-offset 960 --sequence-offset 816 \
  --sample 45 --weight-order first \
  --obj private/extracted/pose45.obj --json private/reports/pose45.json
```

This evaluates one integer sample and exports geometry-only OBJ, with one object
per packet. `--weight-order` is required: `first` puts the leftover weight before
the explicit weights; `last` puts it after them. The selected real pose supports
`first`; this is not a rule for every model. Omit `--animation` and its sample/offset
options to check the bind pose. Without a sequence header, sample time stays
unknown. No looping or interpolation is assumed.

The evaluator uses A as the global rest matrix and B as its inverse. File matrices
use row vectors, with translation in the final row. Local rest is A × parent B;
clip rotations replace local rotations and missing translation tracks retain
local rest translations. Child global transforms are local × parent global.
Each vertex is transformed by B × animated global for its palette-mapped bones,
then blended. Weights are normalized after clamping the inspector's permitted
rounding excess. Source axes and units stay unchanged.

Only rigid affine binds are supported: orthogonality, positive unit determinant,
affine entries and inverse agreement must be within 2e-5. Every bone needs a dense
rotation track. Packed rotations return XYZW using the observed six-byte layout;
a set reserved bit or stored components exceeding unit length is refused.
Input/metadata bounds are inherited from the inspectors, with a further aggregate
200,000-index export cap. Morph records are refused. The extra-record count is
reported without interpreting that data.

JSON records input hashes, sample/time, explicit weight choice, global bone
matrices, counts, posed bounds, maximum displacement and OBJ hash. OBJ has positions
and triangle faces only: no normals, UVs, materials or game shaders. It is a local
research output and is not ready for the Android viewer's static OBJ loader.
Output paths must be new and distinct; discard outputs after any failed write.
No dependencies, app changes or proprietary fixtures are added. See
[the pose checks](verification/tools.md#pose-evaluation).

## Measure a dense animation sequence

```bash
.venv/bin/python tools/anm_sequence.py \
  --model private/extracted/reduced-model.ktmdl \
  --animation private/extracted/selected.anm --core-offset 960 --sequence-offset 816 \
  --weight-order first --json private/reports/sequence-first.json
```

The default range includes every dense sample. Use `--start` and `--end` for an
inclusive subset. Each sample uses the verified pose evaluator; no interpolation
or repeated playback is added. A missing sequence header leaves FPS and time
unknown. The input checks, supported bind/track subset and explicit weight choice
are the same as for one pose.

The report records each sample's bounds, root origins, minimum triangle area,
zero-area face count and a SHA-256 of packet-ordered little-endian float64 XYZ
positions. Adjacent samples report maximum vertex/joint movement and local bone
rotation change, with the relevant vertex or node. Quaternion sign changes do
not count as full rotations. The first sample has no previous-step measurement.
First-to-last vertex/joint distance is separate and does not imply the clip loops.

Duplicate source-position groups measure the largest distance from their first
vertex. A second grouping requires the same merged bone weights as well; it
checks coincident bindings across vertices and packets. Distances are anchor
gaps, not all-pairs diameters. Separate surfaces may share a source position but
use different weights, so their separation is evidence to inspect, not an
automatic failure.

Limits: 512 evaluated samples, 2,000,000 vertex-samples and 5,000,000
triangle-samples, checked before posing. Select a smaller range when necessary.
Only the first, previous and current geometry are retained; full geometry is
not written into the report. Output must be new; omit `--json` for stdout.
The tool reports measurements without an assumed movement threshold or automatic
smoothness verdict. Keep game reports private. See
[the full selected-clip check](verification/tools.md#dense-sequence-check).
