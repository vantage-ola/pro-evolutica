# Project status

The inspection tools are implemented and tested. PES 2013 now launches and runs
in Winlator according to the phone test report. Gameplay stutters and doubled
menu sounds remain unresolved; a sustained benchmark and REA/Ghidra analysis
are pending.

## Verified deliverables

- Python project, isolated `.venv`, pinned test dependencies, and exclusions for
  games, private reports, extracted assets and build outputs.
- `tools/pe_inspect.py`: JSON fingerprint, PE architecture, entry point, sections
  and static imports, with input bounds and CLI errors.
- `tools/asset_inventory.py`: recursive JSON/CSV metadata and magic bytes, with
  symlink/special-file exclusion and explicit partial-scan errors.
- Synthetic PE fixtures generated inside tests; no game executable or asset is
  checked in. Tests cover PE32/PE32+, named/ordinal/delayed imports, malformed
  files, allocation limits, CLI errors and report overwrite protection. Inventory
  tests cover Unicode/CSV quoting, read errors and parent-symlink replacement.
- **44 tests pass** on Python 3.11.9. Commands and results are in
  [verification/tools.md](verification/tools.md). Usage is in [TOOLS.md](TOOLS.md).
- The first phone gameplay report and next controlled tests are recorded in
  [benchmarks/pes2013-baseline.md](benchmarks/pes2013-baseline.md). The reported
  60 FPS is an observed reading, not a measured sustained average.

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
No asset parser or format compatibility has been validated. Full reports remain
under ignored `private/reports/`; no proprietary content is included in source.

## Phone result

- PES 2013 launches after a fresh installation. The earlier
  `rld.dll failed to initialize (E200)` error no longer blocks launch. The specific
  installation change responsible has not been isolated.
- Menus and gameplay work, with reported 60 FPS readings and visible stutters.
- Menu effects sometimes sound doubled; game music sounds normal.
- Winlator version, audio driver, resolution, graphics settings and driver/wrapper
  versions have not been captured. Match duration, frame times and thermal
  behaviour have not been measured. A recorded full-match test is still pending.

## REA, Ghidra and device status

- REA and Ghidra were not found in the checked locations. Their versions and
  provider/client `doctor` checks remain unverified.
- ADB connectivity and Android version remain unverified. Winlator gameplay is
  reported working; the exact configuration remains pending.

## Next three tasks

1. Save the working configuration and isolate the menu-audio problem: check for
   repeated input, test with recording off, then compare audio drivers while
   keeping other settings fixed.
2. Capture versions/settings and a 10-minute match at the working configuration.
   Compare 800 × 600 and 1280 × 720 at low graphics in separate runs. Record frame
   times, audio, input and heat; verify a repeatable result before further tuning.
3. After the gameplay baseline is verified, configure REA/Ghidra with approved
   installs and passing `doctor` checks, then investigate file-loading routines
   and select one asset format supported by a real sample.
