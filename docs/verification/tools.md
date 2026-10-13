# Tool verification

Test environment: Python 3.11.9 on macOS ARM64.

## Automated checks

Command:

```bash
.venv/bin/python -m pytest -q
```

Exact final output, also saved in [tools-tests.txt](tools-tests.txt):

```text
........................................................................ [ 33%]
........................................................................ [ 66%]
........................................................................ [ 99%]
.                                                                        [100%]
217 passed in 1.32s
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
private. Model shape, winding, UV/material mapping and transforms still need an
independent reference before a renderer relies on them.

## Review and limits

Reports preserve parser warnings; an import list with warnings requires further
review. These checks verify diagnostics, AFS extraction, the observed WESYS/zlib
wrapper and the supported flat DXT1 texture workflow on macOS ARM64. They do not
verify script bytecode, other texture formats, full model rendering, REA/Ghidra,
native rendering or device connectivity. The KTMDL checks additionally verify
the supported geometry metadata on two local models; materials, UV values,
triangle area and winding remain unverified. Details and hashes are in
[the model research record](../research/pes2013.md#selected-ktmdl-model-samples).
The manual Winlator report is recorded separately in
[the phone baseline](../benchmarks/pes2013-baseline.md); sustained performance
has not been measured.
