# Local inspection tools

Use Python 3.11 or newer. Both tools read local files; neither executes a game.
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
