# Local native animation playback

The viewer loads and plays the selected 19-bone model and 91-sample clip on the
Note20. This connects the previously checked deformation to a bounded local
file, sample timing and dynamic vertex updates. It is an asset viewer, with
geometry-only lighting and no gameplay systems.

## File and playback behaviour

`tools/animation_export.py` writes our `PEVA0001` format from the supported
KTMDL/ANM pair. All numbers are little-endian. The fixed layout is:

- Eight-byte signature; five uint32 counts: bones, vertices, indices, samples, FPS.
- Float32 centre XYZ and radius, enclosing the bind pose and all dense poses.
- Each bone: int32 parent, then float32 rest-global and inverse-bind matrices.
- Each vertex: float32 XYZ, four uint8 global bone indices, four float64 weights.
- Uint16 triangle-list indices, preserving source winding and packet order.
- Sample-major/bone-major records: six packed rotation bytes, uint8 translation
  flag and float32 XYZ. Absent translations have zero payload and retain rest values.

The 44-byte header and exact expected length are checked before allocation.
Limits are 32 MiB, 128 bones, 65,535 vertices, 600,000 triangle-list indices,
512 samples, two million vertex-samples and 1–240 FPS. Float32 fields must be
finite and within ±1,000,000; radius must be at least 1e-6. Skeleton and weight
validation reuse the checked native module. Every sample is decoded and posed
before acceptance, including verification against the framing radius. Unknown
flags, trailing/truncated data, symlinks, special files and files changed during
reading are refused.

Playback uses accumulated foreground playing nanoseconds, selects
`floor(seconds × FPS)` and clamps to the final sample. Stalls skip samples to
preserve timing. Pausing/backgrounding excludes those intervals; reset rewinds.
There is no interpolation or automatic loop. Options/Space resumes or replays
from the endpoint. Triangle/R or touch hold/release rewinds and pauses while
resetting the camera. Camera movement pauses playback. Normals are recomputed
from each sampled pose's triangles for simple white directional lighting.
Only vertices update; topology and the framing bound stay fixed.

## Verification

Three new Python tests check the original package layout, mapped weights,
translation flags, required timing, work limits and overwrite protection.
One focused CTest target checks known sample geometry/normals, bounded parsing,
later invalid rotations, indices, weights, flags and framing; sample-boundary
rounding, saturation, rewind and regular-file/symlink/FIFO handling.

```bash
.venv/bin/python -m pytest -q
"$ANDROID_HOME/cmake/3.22.1/bin/cmake" --build private/native-tests --parallel 4
"$ANDROID_HOME/cmake/3.22.1/bin/ctest" --test-dir private/native-tests --output-on-failure
"$ANDROID_HOME/cmake/3.22.1/bin/cmake" --build private/native-sanitizers --parallel 4
"$ANDROID_HOME/cmake/3.22.1/bin/ctest" --test-dir private/native-sanitizers --output-on-failure
./android/gradlew -p android --offline --no-daemon :app:assembleDebug
```

Results: **404 Python tests pass in 1.35 seconds**. All **seven native targets
pass** normally and with address/undefined-behaviour sanitizers. After the
Android build exposed a uint64 type difference, an explicit type corrected it;
the affected normal/sanitizer checks pass again. The offline APK build succeeds
in seven seconds, with warnings treated as errors for native code. Existing SDK
XML warnings remain non-fatal. No dependency was installed.

The selected package is 419,205 bytes, with 6,736 vertices and 10,079 triangles.
Its SHA-256 is `c1362860af696a6217aef1fd325269eb34dcfd02347c4b255d348ada8fc05bb0`.
All 612,976 package-loaded vertex positions agree exactly with the previous
verified native bridge, preserving its Python/reference agreement. Every posed
render normal is finite. Input hashes and earlier reference comparisons are in
[the deformation record](native-animation.md). The private probe, package and
comparison output remain outside Git.

## Note20 result

The installed ARM64 APK has SHA-256
`7cfa9cc3e400302d3b92a6324ea71162a9945d031374bce34a2bf6541afef378`.
It contains only the ARM64 SDL, C++ runtime and app libraries, with no game assets.
The supplied file's device hash matches the local package. The app reports
Adreno 650/OpenGL ES 3.2, loads all 91 samples and renders in portrait and landscape.
Screenshots at samples 0, 44 and 90 show coherent distinct poses. The user confirms
coherent motion and PS4 Options/Triangle behaviour; logs also show PS4 X tint,
touch orbit/zoom and reset.

The first sample-zero swap occurs 478.861 ms after native main; package
read/whole-clip validation takes 325.967 ms and mesh/white-texture upload calls
0.710 ms. Sample 90 is reached 3.005 seconds after the first swap and held.
These are one launch's wall times, not an animation FPS or sustained benchmark.

The controlled pause at sample 44 produces byte-identical screenshots taken
more than a second apart. Resume starts from sample 44 and reaches 90; replay
returns to zero. A four-second background interval resumes at sample 37 from
sample 13 after brief foreground/transition time, rather than jumping to the
endpoint. The same process survives. Missing and eight-byte malformed packages
both restore the checked ball and colour texture; screenshots confirm the
fallback, and restoring the valid package restores playback. B refuses the animation scene so it cannot
be reported as the static ball benchmark. Logs and screenshots remain under
ignored `private/device/native-playback/`.

The selected clip's game-action meaning, materials, original animated normals,
interpolation, transitions, looping and gameplay remain unverified. Native
animation frame-time/memory measurements still need their own method and run.
