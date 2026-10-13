# PES 2013: Galaxy Note20 baseline

Status: **playable in Winlator; peak 60 FPS, choppy throughout the match**.
Executable inspection results are recorded in [project status](../STATUS.md).

Target device: Samsung Galaxy Note20,
Snapdragon 865+, Adreno 650. Confirm the actual device and Android version during
the test. Use the local PE report to select a runtime with 32-bit Windows support.

## Initial gameplay report

- Launch succeeds after a fresh installation. The previous error was
  `The dynamic library "rld.dll" failed to initialize (E200)`; its exact cause and
  the installation change that resolved it have not been isolated.
- Menus and gameplay run. **Peak FPS: 60. Gameplay is choppy throughout the
  match.** This is a peak reading, not a sustained average. Average FPS,
  frame-time spikes, test duration and thermal behaviour were not measured.
- Menu sound effects repeat or sound like reverb, sometimes more than twice.
  Crowd audio may also be affected. Music and commentary sound normal.
- Switching to **PulseAudio did not resolve the repeated sound effects**. The
  original audio driver and latency settings were not recorded. Whether input
  repeats or recording affects the issue remains unknown.
- RAM and other usage indicators looked acceptable during the manual test;
  numeric measurements were not captured. The stutter's cause remains unknown.
- Winlator version, runtime, graphics driver/wrapper, resolution
  and graphics settings are pending. No video or device log has been reviewed.

This is a manual test report, not a reproduced benchmark. The phone executable
has not been fingerprinted against the local inspection report.

## Follow-up

Keep the working configuration for gameplay. Further Winlator tuning can be
recorded separately; asset research and original native code can proceed
independently. Rendering, shader compilation, timing and audio buffering are
possible areas to investigate, not established causes.

Keep PES 2015 results in a separate benchmark entry. Its x86 architecture alone
does not establish compatibility with the PES 2013 configuration.

## Optional comparison record

Fill in after the test. Empty fields are pending, not pass results.

```text
Test date / run identifier:
Game / edition / executable SHA-256:
Phone / chipset / Android version:
Winlator version:
Runtime / 32-bit compatibility mode:
Audio driver / average latency:
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
