# Native Android test scene

The triangle established rendering and input on the Note20. The current scene
draws a rotating cube through OpenGL ES 3, with a locally supplied PNG or an
original 64 × 64 checkerboard, depth testing, perspective and per-face shading.
Rotation uses elapsed time;
portrait views move the camera back to keep the cube visible.

Tapping the screen or pressing a gamepad's south button (X on the tested PS4
controller) changes its tint. Android Back closes the app. The renderer stays
separate from SDL's window, input and lifecycle code. No game files are packaged.

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
`Textured cube frame submitted`. A successful build or that log alone does not prove
the image looks correct; keep a screenshot and the manual input result.

## Load a local texture

The debug app reads `texture.png` from its own internal storage at startup.
It uses the checkerboard if that file is missing, unsupported or damaged, and
logs the reason. No storage permission is needed. With one authorised phone:

```bash
adb shell run-as com.proevolutica.viewer mkdir -p files
adb exec-in run-as com.proevolutica.viewer tee files/texture.png \
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

## CPU tests

The geometry, checkerboard, camera math and PNG reader need neither SDL nor
Android to test.
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

The locally extracted PES texture has been verified on the Note20. Its intended
in-game use is still unknown; it remains outside the APK and Git.
