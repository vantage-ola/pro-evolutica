# Native Android triangle

The first native scene draws an original coloured triangle through OpenGL ES 3.
Tapping the screen or pressing a gamepad's south button changes its tint. Android
Back closes the app. The renderer stays separate from SDL's window, input and
lifecycle code. No game files are packaged.

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
./android/gradlew -p android --offline --no-daemon :app:assembleDebug
```

The APK should be at `android/app/build/outputs/apk/debug/app-debug.apk` and
contain only the `arm64-v8a` native libraries. The offline command deliberately
fails when a required dependency is absent; install missing packages explicitly.
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
adb logcat -d -v threadtime -s SDL SDL/APP AndroidRuntime > private/device/triangle.log
adb exec-out screencap -p > private/device/triangle.png
```

Check that the triangle appears without stretching in portrait and landscape.
Tap once and check that its tint changes once. Connect a supported gamepad and
press its south button; check the tint and corresponding input log. Send the app
to the background and resume it, then check Android Back closes it cleanly.

Logs should identify the actual GL renderer/version and include
`Triangle frame submitted`. A successful build or that log alone does not prove
the image looks correct; keep a screenshot and the manual input result.

Verify this scene before adding a textured cube. Loading the locally extracted
PES texture is a later step; its intended in-game use is still unknown.
