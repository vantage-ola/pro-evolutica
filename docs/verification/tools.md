# Tool verification

Test environment: Python 3.11.9 on macOS ARM64.

## Automated checks

Command:

```bash
.venv/bin/python -m pytest -q
```

Exact final output, also saved in [tools-tests.txt](tools-tests.txt):

```text
........................................................................ [ 67%]
..................................                                       [100%]
106 passed in 0.90s
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

## Review and limits

Reports preserve parser warnings; an import list with warnings requires further
review. These checks verify the diagnostics, AFS extraction and the observed
WESYS/zlib wrapper on macOS ARM64. They do not verify contained script bytecode,
mesh/texture formats, REA/Ghidra, native rendering or device connectivity.
The manual Winlator report is recorded separately in
[the phone baseline](../benchmarks/pes2013-baseline.md); sustained performance
has not been measured.
