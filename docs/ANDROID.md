# Android build

The current Android target is an ARM64 asset viewer and runtime prototype. It is not yet a playable football game.

## Requirements

- JDK 21
- Android SDK platform 35 and Build Tools 34.0.0
- Android NDK 28.2.13676358
- CMake 3.22.1
- SDL3 3.4.18 Android archive, with `SDL3-3.4.18.aar` at `android/app/libs/SDL3-3.4.18.aar`

Set `JAVA_HOME` and `ANDROID_HOME`. If CMake is outside the SDK, set `cmake.dir` in the ignored `android/local.properties` file. Build from the repository root:

```bash
export GRADLE_USER_HOME="$PWD/private/gradle"
./android/gradlew -p android --no-daemon :app:assembleDebug
```

The debug APK is created at `android/app/build/outputs/apk/debug/app-debug.apk`. The first build needs network access to download uncached Gradle dependencies. Later builds can use `--offline` once dependencies are cached.

## Tests

Run the Python checks with:

```bash
.venv/bin/python -m pytest -q
```

Build and run the native C++ checks with the CMake and Ninja executables from the Android SDK:

```bash
"$ANDROID_HOME/cmake/3.22.1/bin/cmake" -S native -B private/native-tests -G Ninja \
  -DCMAKE_MAKE_PROGRAM="$ANDROID_HOME/cmake/3.22.1/bin/ninja"
"$ANDROID_HOME/cmake/3.22.1/bin/cmake" --build private/native-tests
"$ANDROID_HOME/cmake/3.22.1/bin/ctest" --test-dir private/native-tests --output-on-failure
```

## Install on a device

With USB debugging enabled and an authorised Android device connected:

```bash
adb devices -l
adb install -r android/app/build/outputs/apk/debug/app-debug.apk
adb shell am start -n com.proevolutica.viewer/.ViewerActivity
```

Keep game files, extracted assets, device logs, and test outputs in ignored local storage such as `private/`.
