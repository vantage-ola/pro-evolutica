# Native Android test scene

The triangle established rendering and input on the Note20. The viewer now
loads one checked static mesh and its PNG from local app storage. The selected
PES 2013 ball has been rendered on the Note20 with its colour texture. The
original rotating cube remains available when no supported mesh is supplied.
Both scenes use OpenGL ES 3, depth testing, back-face culling and perspective.
The mesh has simple directional lighting; the cube keeps its per-face shading.
Geometry is framed around its centre and radius. Portrait views move the camera
back to keep the scene visible, and rotation uses elapsed time.

A short screen tap or a gamepad's south button (X on the tested PS4 controller)
changes its tint. Android Back closes the app. The renderer stays
separate from SDL's window, input and lifecycle code. No game files are packaged.
Space or the gamepad Start button (Options on the tested PS4) pauses/resumes
rotation for inspection. Moving the camera pauses rotation automatically.

## Inspect the model

- Drag one finger to orbit around the model's centre. Dragging does not change tint.
- Pinch two fingers apart to zoom in, or together to zoom out.
- Hold one finger still for at least 0.6 seconds, then release to reset.
- PS4: left stick orbits; right stick up/down zooms; Triangle resets.
- PS4 X changes tint; Options toggles rotation.
- Keyboard: arrows orbit; `=` zooms in; `-` zooms out; R resets; Space toggles rotation.

Reset restores the initial camera, white tint and initial model angle, with
rotation paused. Camera movement also pauses rotation so the view stays still
for inspection; resume with Options or Space. Stick input has an 18% dead zone
and uses elapsed time. The camera stays centred, limits vertical orbit before
the poles and bounds zoom to keep the scene visible. It accounts for the cube's
larger radius as well as the normalised mesh. Resize/background/cancel events
clear touch gestures so a stale finger cannot trigger a tap or reset. The
resizable SDL window explicitly allows portrait and landscape rotation.

## Measure a native run

Keyboard B (or `adb shell input keyevent 30`) starts a fixed benchmark. It resets
the camera/tint/model angle, resumes rotation, warms up for five seconds, then
records at least 60 seconds. Input, stick movement, backgrounding or resizing
aborts the capture. A second B press also cancels it. Keep the phone in one
orientation and the app in the foreground during the run.

The app keeps up to 20,000 raw frame samples in a preallocated buffer and writes
`files/benchmark.json` and `files/benchmark-frames.csv` afterward. Starting a new
capture removes the preceding report; an aborted run publishes no completed
summary. These files contain measurements, not game bytes. No per-frame log or
file write happens during recording. JSON is published after its CSV; save the
results elsewhere before starting another capture.

After building/installing the intended debug APK and loading the checked model:

```bash
python3 scripts/benchmark_android.py --serial YOUR_ADB_SERIAL \
  --output private/device/native-baseline --runs 3
```

Use a new output directory each time. The standard-library collector checks the
installed APK hash against the local APK, records both asset hashes and display
state, and starts a fresh viewer process for each run. It waits for the fixed
capture, saves process-filtered logs and pulls the raw CSV and JSON. It refuses
interrupted/restarted runs, missing completion evidence and non-mesh reports.
Memory, battery and thermal snapshots are taken before warmup and after the
report, outside the timed window. It does not install packages or change device
settings. Snapshots and full logs belong under ignored private storage.

Frame intervals run from one completion to the next, including event polling,
update, GL submission, swap wait and the existing fallback delay when vsync is
unavailable. Submit time covers the draw API calls and GL error check; swap time
covers `SDL_GL_SwapWindow`. These are wall times, not GPU timer measurements or
compositor presentation times. FPS is frame count divided by total interval
time. Percentiles use nearest rank; threshold counts are strictly greater than
the listed 16.67/33.33/50 ms limits. Raw nanosecond CSV values allow independent
recalculation. Warmup and report-writing frames are excluded.

Startup logs/report fields separately measure file read/decode, GL upload calls,
native creation and native-main-to-first-successful-swap. They exclude Java/process
startup before native main. Fresh process launches can still use warm OS file
caches. `dumpsys meminfo` snapshots describe process memory at those points, not
peak memory or a leak test. See [the native measurement record](verification/android.md#native-measurements) for
actual device results and conditions.

## Build requirements

- JDK 21. The included wrapper selects Gradle 8.9.
- Android SDK platform 35, Build Tools 34.0.0 and NDK 28.2.13676358 (r28c).
- CMake 3.22.1, including its Ninja executable.
- SDL3 3.4.18's Android archive, placed at
  `android/app/libs/SDL3-3.4.18.aar`.

SDL's official [Android release archive](https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/SDL3-devel-3.4.18-android.zip)
has SHA-256
`e09e4d6593335d089ba124375e8b8abd0fd75173dffcd2c17012b45af121d4e9`.
Unpack its AAR after checking the archive hash. SDL uses the
[zlib licence](https://github.com/libsdl-org/SDL/blob/release-3.4.18/LICENSE.txt).
Its [Android integration guide](https://github.com/libsdl-org/SDL/blob/release-3.4.18/docs/README-android.md#using-the-sdl3-android-archive-aar)
describes the AAR/Prefab interface used here. Dependencies remain outside Git.

Set `JAVA_HOME` to the JDK and `ANDROID_HOME` to the SDK. If CMake is outside the
SDK, set `cmake.dir=/absolute/path/to/cmake/3.22.1` in ignored
`android/local.properties`. Build from the repository root:

```bash
export GRADLE_USER_HOME="$PWD/private/gradle"
./android/gradlew -p android --no-daemon :app:assembleDebug
```

The APK should be at `android/app/build/outputs/apk/debug/app-debug.apk` and
contain only the `arm64-v8a` native libraries. The first build needs network
access to fetch missing Gradle dependencies, including the Prefab CLI used to
connect SDL's AAR to CMake. Downloads go into the project-local Gradle cache.
After a successful build, add `--offline` to build using only cached dependencies;
it fails if a required dependency is missing.
The wrapper itself downloads Gradle if its distribution is not cached, even with
`--offline`; its ZIP is pinned to the official SHA-256 in wrapper properties.
Build verification and current blockers are in
[verification/android.md](verification/android.md).

## Phone check

With USB debugging enabled and one authorised phone connected:

```bash
adb devices -l
adb install -r android/app/build/outputs/apk/debug/app-debug.apk
adb shell am start -n com.proevolutica.viewer/.ViewerActivity
mkdir -p private/device
adb logcat -d -v threadtime -s SDL SDL/APP AndroidRuntime > private/device/cube.log
adb exec-out screencap -p > private/device/cube.png
```

Check that the checkerboard cube rotates without stretching in portrait and landscape.
Tap once and check that its tint changes once. Connect a supported gamepad and
press its south button; check the tint and corresponding input log. Send the app
to the background and resume it, then check Android Back closes it cleanly.

Logs should identify the actual GL renderer/version and include
`Textured mesh frame submitted` or `Textured cube frame submitted`. A successful
build or that log alone does not prove
the image looks correct; keep a screenshot and the manual input result.

## Load a local texture

The debug app reads `texture.png` from its own internal storage at startup.
It uses the checkerboard if that file is missing, unsupported or damaged, and
logs the reason. No storage permission is needed. With one authorised phone:

```bash
adb shell run-as com.proevolutica.viewer mkdir -p files
adb shell -T run-as com.proevolutica.viewer tee files/texture.png \
  < private/extracted/dt08-entry-00002.png > /dev/null
adb shell run-as com.proevolutica.viewer sha256sum files/texture.png
adb shell am force-stop com.proevolutica.viewer
adb shell am start -n com.proevolutica.viewer/.ViewerActivity
```

Replace the local input path with your own supported PNG. The `run-as` command
requires this debug build. Updates installed with `adb install -r` keep the image;
uninstalling the app removes it. To return to the checkerboard, remove only
`files/texture.png` with `run-as`, then restart the app.

The portable reader supports non-interlaced, eight-bit RGBA PNGs with filter
types 0–4. It caps input at 16 MiB, dimensions at 4096 per side, pixels at
4,194,304 and chunks at 4096. It verifies every chunk CRC, chunk order and exact
zlib output/input lengths. Symlinks and special files are rejected. RGB,
palette, grayscale, 16-bit and animated PNGs are unsupported. Metadata, including
colour profiles, is checked for CRC but not applied. Alpha is decoded unchanged;
the cube currently uses opaque rendering without blending.

The implementation follows the [PNG format and filter definitions](https://www.w3.org/TR/2003/REC-PNG-20031110/)
and uses the existing system/NDK zlib library. SDL's
[internal-storage API](https://wiki.libsdl.org/SDL3/SDL_GetAndroidInternalStoragePath)
selects the Android path; the reader has no Android or SDL dependency.

## Load the checked static mesh

Create the local OBJ using [the static exporter](TOOLS.md#export-a-supported-static-ktmdl-packet).
Supply the selected colour PNG explicitly; the app does not resolve game bundle
IDs or load material files.

```bash
adb shell run-as com.proevolutica.viewer mkdir -p files
adb shell -T run-as com.proevolutica.viewer tee files/model.obj \
  < private/extracted/ball.obj > /dev/null
adb shell -T run-as com.proevolutica.viewer tee files/model.png \
  < private/extracted/ball-colour.png > /dev/null
adb shell run-as com.proevolutica.viewer sha256sum files/model.obj files/model.png
shasum -a 256 private/extracted/ball.obj private/extracted/ball-colour.png
adb shell am force-stop com.proevolutica.viewer
adb shell am start -n com.proevolutica.viewer/.ViewerActivity
```

Compare both device hashes with the local files before relying on a render.
Use `shell -T` for the transfer: an `exec-in` transfer truncated the model during
testing. The reader rejected that partial file and retained the cube.

The OBJ reader accepts only our static export subset: equal-size position,
UV and normal arrays; positive, matching `v/vt/vn` indices; triangle faces;
and at most one object. It rejects materials, groups, smoothing directives,
polygons, negative indices and separate index streams. It never opens files
named inside an OBJ. Input is capped at 32 MiB, 500,000 lines, 512 bytes per line,
65,535 vertices and 600,000 triangle-list indices. Attribute magnitudes above
1,000,000, normals/radii below 1e-6, non-finite values, invalid indices and
zero-area faces are refused. The file must be regular, stable and not a symlink.

Mesh positions, normals and UVs remain unchanged in the reader. Centre/radius
are computed for framing; the renderer recentres and scales uniformly. The
checked KTMDL UVs address the top-first PNG directly; the original cube's UVs
retain their earlier vertical flip. This is not a general OBJ material viewer.
Normal/specular maps, proprietary shaders, skinning and animation are unsupported.

Missing/damaged `model.obj` uses the cube and its existing `texture.png` path.
A valid mesh with missing/damaged `model.png` uses the original checkerboard on
that mesh. Removing only `files/model.obj` with `run-as` and restarting returns
to the cube. No game asset is included in the APK.

## CPU tests

The geometry, checkerboard, camera math, frame statistics, PNG and OBJ readers
need neither SDL nor Android to test.
The host PNG target uses the installed system zlib headers/library.
With `ANDROID_HOME` pointing to the installed SDK:

```bash
"$ANDROID_HOME/cmake/3.22.1/bin/cmake" -S native -B private/native-tests -G Ninja \
  -DCMAKE_MAKE_PROGRAM="$ANDROID_HOME/cmake/3.22.1/bin/ninja"
"$ANDROID_HOME/cmake/3.22.1/bin/cmake" --build private/native-tests
"$ANDROID_HOME/cmake/3.22.1/bin/ctest" --test-dir private/native-tests --output-on-failure
```

Tests check face winding, texture coordinates, opaque checkerboard pixels, known
perspective/rotation results, visibility through a full rotation at five aspect
ratios, and rejection of non-finite or invalid camera inputs. The PNG target uses
original generated fixtures for exact pixel comparisons across all filters,
truncation, CRC errors, allocation bounds, invalid chunk order, corrupt zlib
streams and regular-file restrictions. It has a ten-second timeout so a
regression in the FIFO check cannot block the test run indefinitely.

The camera target checks orbit handedness, default framing, zoom/pitch limits,
stick dead zones and elapsed-time movement. It projects cube vertices and sphere
samples across portrait/landscape aspects, camera angles and zoom limits. Touch
cases cover tap jitter, drag, hold/reset, pinch, cancellation and extra fingers.

The frame-statistics target checks known summaries and spikes, exact threshold
boundaries, warmup exclusion, clock/stage errors, elapsed duration, cancellation
and the sample cap.

The mesh target checks original known positions, normals, UVs and face indices,
framing through a full rotation at five aspect ratios, CRLF/comments/exponents,
unsupported records, malformed faces, finite values, degeneracy, allocation
limits and regular-file/symlink/FIFO restrictions. It also has a ten-second
timeout. All five targets pass normally and with address/undefined-behaviour
sanitizers. Device results and asset checksums are in
[verification/android.md](verification/android.md#static-ball-model).

The earlier grayscale texture's in-game use remains unknown. The selected ball
and its colour texture now have verified native rendering; all extracted assets
remain outside the APK and Git.
