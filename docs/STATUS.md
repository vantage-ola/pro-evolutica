# Project status

Updated: 13 October 2026 (Africa/Lagos).

**The inspection tools are implemented and tested. Milestone 1 remains open until the phone's
first gameplay result is recorded, including a crash if that is the outcome.**
No later milestone implementation has started.

## Repository inspection

The initial working tree was clean and contained only `README.md`; no tools,
tests, dependencies or Android code existed. The full README is the specification.
The first-session section includes asset inventory in the initial software work,
even though the broader plan also lists it under Milestone 3.

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
- A blank phone benchmark template is ready in
  [benchmarks/pes2013-baseline.md](benchmarks/pes2013-baseline.md).
- Local Verity proof chains link both tools to their synthetic tests and saved
  test output. Nothing has been published to Walrus or any external service.

## Local game observations

The owner supplied a `pro-evo` folder containing PES 2013 and PES 2015. Both
executable reports agree with `file` on **PE32, x86, 32-bit Windows**. Their SHA-256
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

## REA, Ghidra and device status

- REA: no `rea` command on PATH and no cached `rea-agents` installation found in
  the checked npm cache. Version unavailable; provider/client `doctor` checks
  **not run**, since they would require an unapproved installation.
- Ghidra: no executable on PATH, installation in `/Applications`, matching
  download name, or configured `GHIDRA_INSTALL_DIR` found in the checked locations.
  Version unavailable. This is not proof that no copy exists elsewhere.
- Node v25.8.1 and npm 11.11.0 were observed. Node 25 is outside the support ranges
  stated in the README; verify a supported version when REA setup is approved.
- Java reports Temurin OpenJDK 26.0.2.1. Ghidra's native decompiler and REA
  integration remain unverified. Global Codex configuration is unchanged.
- ADB 37.0.1 is installed. `adb devices` could not start its server inside the
  sandbox (`could not install *smartsocket* listener: Operation not permitted`).
  Device connectivity, Winlator, Android version and gameplay remain unverified.

## Next three tasks

1. On 14 October, run PES 2013 on the Note20 with 32-bit Windows support,
   Turnip + DXVK, 800 × 600 and low graphics. Record a boot/gameplay result in the
   baseline template, even if it fails.
2. Capture the installed versions, settings and evidence. If rendering fails,
   record a separate WineD3D attempt; if it works, capture a 10-minute match.
   Verify that record before moving to the performance milestone.
3. Approve and provide the local REA/Ghidra setup when ready, then run both
   `doctor` checks. Start evidence-based binary investigation only after the
   current milestone gate and integration checks pass.
