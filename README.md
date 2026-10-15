# pro-evolutica

A native Android porting effort for PES 2013. The aim is to run the original game on Android, preserving its gameplay and assets where possible and replacing platform-specific parts only when needed.

The repository currently contains an Android 3D viewer, PES asset readers, and local inspection tools. It does not yet run a football match. Commercial game files are not included; use your own local copy and keep it outside version control.

## Build the Android app

See [Android build instructions](docs/ANDROID.md). The app targets `arm64-v8a` and uses SDL3 with the Android NDK.

## Run checks

```bash
.venv/bin/python -m pytest -q
```

Android build and device instructions are in [docs/ANDROID.md](docs/ANDROID.md).
