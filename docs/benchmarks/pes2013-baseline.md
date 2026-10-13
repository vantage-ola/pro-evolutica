# PES 2013: Galaxy Note20 baseline

Status: **launch and gameplay reported working; sustained benchmark pending**.
Executable inspection results are recorded in [project status](../STATUS.md).

Target device: Samsung Galaxy Note20,
Snapdragon 865+, Adreno 650. Confirm the actual device and Android version during
the test. Use the local PE report to select a runtime with 32-bit Windows support.

## Initial gameplay report

- Launch succeeds after a fresh installation. The previous error was
  `The dynamic library "rld.dll" failed to initialize (E200)`; its exact cause and
  the installation change that resolved it have not been isolated.
- Menus and gameplay run. FPS readings reach 60, with stutters. Average FPS,
  frame-time spikes, test duration and thermal behaviour were not measured.
- Menu sound effects sometimes play with a doubled/stuttering sound. Game music
  sounds normal. It is not yet known whether menu input also repeats or whether
  recording affects the issue.
- Winlator version, audio driver, runtime, graphics driver/wrapper, resolution
  and graphics settings are pending. No video or device log has been reviewed.

This is a manual test report, not a reproduced benchmark. The phone executable
has not been fingerprinted against the local inspection report.

## Menu-audio comparison

1. Save screenshots of the working container and shortcut settings, including
   Winlator version and the selected audio driver.
2. With screen recording off, navigate the same menu with ten separate taps.
   Note whether each tap moves the selection once or twice, and whether its sound
   repeats. If selection also repeats, check the input mapping before attributing
   the problem to audio.
3. If selection moves once but sound repeats, compare ALSA and PulseAudio if both
   are available in the installed Winlator version. Restart the container between
   runs, use the same menu and change no other settings. Record each result and
   keep the better configuration.
4. Repeat with screen recording using device audio only. Note whether the problem
   is audible during play, in the saved recording, or both.

If the remaining symptom is crackling, Winlator's
[audio guidance](https://github.com/brunodev85/winlator#useful-tips) suggests
increasing average audio latency, with 90 ms as an example for an older game.
Try that as a separate comparison if the setting is available; it is not a
confirmed fix for PES 2013's doubled menu sound.

## Next measured run

1. Record the working configuration before changing it. Capture Winlator version,
   runtime/32-bit support, audio driver, graphics driver/wrapper versions,
   resolution and in-game graphics settings.
2. If using DXVK, set `DXVK_HUD=fps,frametimes,compiler,version,devinfo` in the
   container or game shortcut environment. The
   [DXVK HUD](https://github.com/doitsujin/dxvk#hud) shows FPS, a frame-time graph,
   shader compilation activity and version/device information. Check whether
   stutters coincide with compilation; coincidence alone does not establish the
   cause. A displayed FPS reading is not a match average.
3. Record at least 10 minutes of real play, including menus, intro, set pieces and
   replay, using device audio only. Note stutters, control response, audio and heat
   toward the end. Leave unavailable measurements marked `not measured`.
4. Repeat the same match setup at 800 × 600 and 1280 × 720, low graphics, in
   separate runs. Keep other settings fixed and start at a similar phone
   temperature. Keep the most stable result before testing higher graphics.

Keep PES 2015 results in a separate benchmark entry. Its x86 architecture alone
does not establish compatibility with the PES 2013 configuration.

## Measured run record

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
