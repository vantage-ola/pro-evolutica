# Tool verification

Test environment: Python 3.11.9 on macOS ARM64.

## Automated checks

Command:

```bash
.venv/bin/python -m pytest -q
```

Exact final output, also saved in [tools-tests.txt](tools-tests.txt):

```text
............................................                             [100%]
44 passed in 0.55s
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

## Review and limits

Reports preserve parser warnings; an import list with warnings requires further
review. These checks verify the diagnostics on macOS ARM64. They do not verify
asset formats, extracted assets, REA/Ghidra, Winlator boot, rendering, performance
or device connectivity. REA health checks and the phone benchmark remain pending.
