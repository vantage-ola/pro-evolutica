# Project status

PES 2013 runs in Winlator: peak 60 FPS, with choppy gameplay throughout the match.
Repeated menu effects remain unresolved. Asset research now has a tested AFS
reader and a verified raw entry extraction. See [DECISION_001.md](DECISION_001.md)
for the next development direction.

## Verified deliverables

- Python project, isolated `.venv`, pinned test dependencies, and exclusions for
  games, private reports, extracted assets and build outputs.
- `tools/pe_inspect.py`: JSON fingerprint, PE architecture, entry point, sections
  and static imports, with input bounds and CLI errors.
- `tools/asset_inventory.py`: recursive JSON/CSV metadata and magic bytes, with
  symlink/special-file exclusion and explicit partial-scan errors.
- `tools/afs_inspect.py`: bounded AFS entry-table inspection, archive hashes and
  extraction of one raw entry by index, with an output checksum.
- Synthetic PE fixtures generated inside tests; no game executable or asset is
  checked in. Tests cover PE32/PE32+, named/ordinal/delayed imports, malformed
  files, allocation limits, CLI errors and report overwrite protection. Inventory
  tests cover Unicode/CSV quoting, read errors and parent-symlink replacement.
- **73 tests pass** on Python 3.11.9. Commands and results are in
  [verification/tools.md](verification/tools.md). Usage is in [TOOLS.md](TOOLS.md).
- The phone gameplay report is recorded in
  [benchmarks/pes2013-baseline.md](benchmarks/pes2013-baseline.md). The reported
  60 FPS is a peak reading, not a measured sustained average.

## Local game observations

The inspected PES 2013 and PES 2015 executable reports agree with `file` on
**PE32, x86, 32-bit Windows**. Their SHA-256
values agree with independent `shasum -a 256` runs:

- PES 2013: `5b1dd4fc86b6e92696762caa0f92e4e59f026858131d1be6a0418d1d2b275694`.
  19,916,288 bytes; 19 imported DLL descriptors, 330 reported functions.
- PES 2015: `aaa537541c2bae0742016dfd3c0f4c908dff3e5fab750a006419ab54bc4434ea`.
  39,111,472 bytes; 24 imported DLL descriptors, 623 reported functions.

PES 2013 produced a `pefile` warning that section 6 has both write and execute
flags. The suggested packing explanation is a parser hypothesis, not a verified
finding about this executable. PES 2015 produced no parser warnings.

The PES 2013 inventory scanned 42 files, 6,206,731,485 bytes, with no read errors or
skipped entries. It observed 19 `.img` and 13 `.sfd` files, among other extensions.
All 19 `.img` files passed AFS index inspection: 55,593 slots, 1,273 empty.
One 1,840-byte entry from `dt10.img` was extracted and compared byte-for-byte with
an independent source read. Its payload remains undecoded. Details and hashes
are in [research/pes2013.md](research/pes2013.md). Full reports and extracted bytes
remain under ignored `private/`; no proprietary content is included in source.

## Phone result

- PES 2013 launches after a fresh installation. The earlier
  `rld.dll failed to initialize (E200)` error no longer blocks launch. The specific
  installation change responsible has not been isolated.
- Menus and gameplay work, with peak 60 FPS and choppiness throughout the match.
- Menu effects repeat or reverberate, sometimes more than twice; crowd audio may
  also be affected. Music and commentary sound normal. Switching to PulseAudio
  produced the same issue; the original driver was not recorded.
- Winlator version, resolution, graphics settings and driver/wrapper versions
  have not been captured. Average FPS, frame times and thermal behaviour have not
  been measured. Further benchmarking is optional for the current asset work.

## REA, Ghidra and device status

- REA and Ghidra were not found in the checked locations. Their versions and
  provider/client `doctor` checks remain unverified.
- ADB connectivity and Android version remain unverified. Winlator gameplay is
  reported working; the exact configuration remains pending.

## Next three tasks

1. Verify the observed `WESYS` entry wrapper and implement a bounded decoder with
   synthetic tests if the format evidence supports it.
2. Identify and inspect one decoded texture, mesh or metadata sample; document
   supported versions and verify its output before adding rendering code.
3. Check the Android SDK/NDK/SDL3 setup and build an original ARM64 test scene.
   Device installation and rendering will need separate verification.
