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
