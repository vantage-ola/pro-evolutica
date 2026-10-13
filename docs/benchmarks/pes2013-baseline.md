# PES 2013: Galaxy Note20 baseline

Status: **not tested on the phone**. Executable inspection results are recorded
in [project status](../STATUS.md).

Target device: Samsung Galaxy Note20,
Snapdragon 865+, Adreno 650. Confirm the actual device and Android version during
the test. Use the local PE report to select a runtime with 32-bit Windows support.

## First manual run

1. Install Winlator from the
   [original releases](https://github.com/brunodev85/winlator/releases), and record
   the version installed. Make your local PES 2013 directory available to its
   container.
2. Start with Turnip + DXVK, 800 × 600, low game graphics settings. Record the
   runtime, 32-bit support mode, driver and wrapper versions before launching.
3. Check menus, kickoff and a full match. Save a screenshot or log if launch fails,
   with the exact error. If rendering fails, make a separate WineD3D run and record
   its settings and result.
4. If the match runs, record at least 10 minutes covering an intro, set pieces and
   replay. Record the FPS measurement method; leave unavailable measurements
   marked `not measured`. Note heat and performance after 10–15 minutes.

Keep PES 2015 results in a separate benchmark entry. Its x86 architecture alone
does not establish compatibility with the PES 2013 configuration.

## Run A: Turnip + DXVK

Fill in after the test. Empty fields are pending, not pass results.

```text
Test date / run identifier:
Game / edition / executable SHA-256:
Phone / chipset / Android version:
Winlator version:
Runtime / 32-bit compatibility mode:
Turnip version:
DirectX wrapper and version:
Screen resolution / game graphics settings:
Controller configuration:
Launch: pass/fail + errors:
Gameplay: menu / kickoff / full match / cutscene / replay:
Average FPS (measurement method):
1% low FPS or frame-time observations:
Thermal behaviour after 10–15 min:
Audio / input / graphical problems:
Evidence (local video or log path, not proprietary files):
Conclusion / next test:
```

Copy this block for each additional run, including a WineD3D fallback. Change one
setting at a time. Keep raw videos, screenshots and logs in ignored local storage.
