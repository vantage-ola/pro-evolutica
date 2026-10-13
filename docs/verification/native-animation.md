# Portable skeletal deformation

The C++ module in `native/animation/skeleton.h` and `.cpp` now evaluates the
verified rigid skeletal subset and deforms vertex positions. It has no SDL,
OpenGL, Android or file-system dependency. The original deformation result is recorded here. Local loading, timed playback
and the subsequent Note20 result are in [the playback record](native-playback.md).

## Behaviour

`Skeleton` validates 1–128 bones, parent ranges, an acyclic hierarchy, rigid
affine rest/inverse-bind matrices and inverse agreement within 2e-5. It prepares
parent order and local rest translations once. Bone storage need not put parents
first. `evaluate` requires one local rotation per bone; missing translations
retain the local rest values. Quaternions use XYZW and file matrices use row
vectors. Local × parent global produces the animated global transform; inverse
bind × animated global produces the skin transform.

`decode_rotation` implements the observed six-byte dense packing. Unknown
reserved bits and invalid unit-length reconstruction are refused. Explicit
local quaternions must be finite and unit length within 2e-5; small rounding
drift is normalized. Computation uses double precision.

`skin_positions` supports 1–65,535 vertices and up to four bone contributions
per vertex. Bone indices must already be mapped through the packet palette;
the caller must resolve the residual-weight order. Finite, nonnegative weights
must sum to one within 2e-6 and are normalized for rounding. Nonzero weights
require in-range bone indices. Non-finite inputs/results are refused. This
module only returns positions: normals, topology, materials and sample timing
remain outside its scope.

## Focused checks

One new CTest target contains four original check groups:

- Bind recovery, rotation about a translated joint and known weighted movement.
- Parent rotation/root translation, unordered bones and replacement of a rotated
  rest orientation by the animated local rotation.
- Literal packed rotations and refusal of unsupported packed values.
- Invalid hierarchy, bind matrices, incomplete poses, quaternions, translations,
  weights, bone indices and vertex limits.

With `ANDROID_HOME` set to the installed SDK:

```bash
"$ANDROID_HOME/cmake/3.22.1/bin/cmake" -S native -B private/native-tests
"$ANDROID_HOME/cmake/3.22.1/bin/cmake" --build private/native-tests --parallel 4
"$ANDROID_HOME/cmake/3.22.1/bin/ctest" --test-dir private/native-tests --output-on-failure

"$ANDROID_HOME/cmake/3.22.1/bin/cmake" -S native -B private/native-sanitizers \
  -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer'
"$ANDROID_HOME/cmake/3.22.1/bin/cmake" --build private/native-sanitizers --parallel 4
"$ANDROID_HOME/cmake/3.22.1/bin/ctest" --test-dir private/native-sanitizers --output-on-failure
```

Builds return 0 with warnings treated as errors. All **six CTest targets pass**
normally and with address/undefined-behaviour sanitizers. No new Python tests
were added; the unchanged Python suite's latest result remains 401 passing.

## Real-clip comparison and Android compile

A private numerical bridge supplies the selected 19-bone model, resolved
residual-first weights and 91 dense samples to the C++ module. The bridge's
temporary binary layout is test plumbing, not a production asset format. Its
source, input and output stay under ignored `private/research/native-skeleton/`.

All 612,976 posed vertices are compared with the verified Python evaluator and
the separate quaternion/vector reference over pinned-reader data:

- Maximum vertex distance from Python: `1.350930e-15` source units.
- Maximum global matrix component difference from Python: `7.771561e-16`.
- Maximum decoded quaternion component difference from Python: `1.110223e-16`.
- Maximum vertex distance from the separate reference: `1.233889e-6` source units.

The differences from Python are floating-point rounding. Reference differences
retain the previously measured effect of stored inverse-bind rounding. This is
a full sampled numerical comparison, not a Blender/game runtime or device test.

The installed NDK also compiles the source without warnings for the app's
minimum API 23:

```bash
"$ANDROID_HOME/ndk/28.2.13676358/toolchains/llvm/prebuilt/darwin-x86_64/bin/aarch64-linux-android23-clang++" \
  -std=c++17 -Wall -Wextra -Wpedantic -Werror \
  -c native/animation/skeleton.cpp \
  -o private/research/native-skeleton/skeleton-android-arm64.o
file private/research/native-skeleton/skeleton-android-arm64.o
```

Compilation returns 0; `file` reports an ELF 64-bit little-endian relocatable
ARM aarch64 object. No dependency was installed. For this deformation-only step,
README, renderer and APK were unchanged, and no game data enters Git. The
subsequent bounded loading and sample-selection implementation holds the final
sample and now passes device playback checks; see the linked playback
record. Interpolation and automatic looping remain unsupported.
