# Portable base-archive resource loading

`native/assets/resource.h` and `.cpp` implement the checked base-archive route
in C++17. The module uses zlib and POSIX file reads, with no SDL, graphics or
Android API dependency. The viewer now invokes it through an optional diagnostic
on the Note20. That diagnostic verifies archive loading and decoding. The
separate [T-key texture preview](native-texture.md#note20-preview) now connects
the ball's decoded texture to the renderer. Neither executes the original engine.

## API and supported behaviour

```cpp
#include "assets/resource.h"

// img_directory is supplied by the caller. This sample ID is derived.
auto resource = assets::load_resource(img_directory, 0x0b000011u);
// resource.bytes contains the decoded payload.
// id, entry_count, entry_offset and entry_size describe its source.
```

The high byte selects `dt0b.img` through `dt0e.img` for selectors `0x0b` through
`0x0e`. The lower 24 bits select a zero-based entry. Other selectors, missing
archives and empty slots throw an exception. The entire AFS index is validated
before reading the selected payload. Only its actual length is read; sector
padding and unrelated payloads are excluded. No whole-archive hash is computed.

`decode_wesys` is also available for an in-memory entry. It accepts the observed
stored `00 01 00` and zlib `00 01 01` wrappers. Stored data requires a zero field
at offset 12. Both forms allow at most 15 trailing zero padding bytes. Compressed
input must contain exactly one complete, checksum-valid zlib stream with the
declared decoded length. One spare output byte detects false small declarations
without unbounded expansion.

Limits match the Python loader: 8 GiB per archive, 100,000 index entries, 64 MiB
raw WESYS input and 64 MiB decoded output. Offset/length addition uses 64 bits.
Final archive paths cannot be symlinks or special files. Reads retry interrupted
system calls and reject truncation. Size and nanosecond modification/change
timestamps are checked before and after reading; these checks detect common
changes but do not provide a file snapshot. The library returns bytes in memory
and creates no output files.

Alternate providers, loose files, asynchronous scheduling, original ownership
rules and interpretation of the decoded payload remain outside this component.

## Host verification

With `ANDROID_HOME` pointing to the installed SDK:

```bash
"$ANDROID_HOME/cmake/3.22.1/bin/cmake" -S native -B private/native-tests
"$ANDROID_HOME/cmake/3.22.1/bin/cmake" --build private/native-tests --parallel 4
"$ANDROID_HOME/cmake/3.22.1/bin/ctest" --test-dir private/native-tests --output-on-failure

"$ANDROID_HOME/cmake/3.22.1/bin/cmake" -S native -B private/native-sanitizers \
  -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer'
"$ANDROID_HOME/cmake/3.22.1/bin/cmake" --build private/native-sanitizers --parallel 4
"$ANDROID_HOME/cmake/3.22.1/bin/ctest" --test-dir private/native-sanitizers --output-on-failure
```

Both builds return 0 with warnings treated as errors. All **eight CTest targets
pass**, normally and with address/undefined-behaviour sanitizers. The single new
resource target contains 134 checks using original synthetic bytes. They cover
stored/compressed/empty output, padding, truncation, versions, corrupt checksums,
concatenated streams, false size declarations, archive selection, invalid and
overlapping index ranges, empty slots, allocation limits and file-type handling.
An invalid unselected entry is rejected; a valid unrelated payload is ignored.
Sparse oversized files exercise the file limits without allocating gigabytes.

Two local samples were compared in normal and sanitizer builds. Every decoded
byte agrees with both the Python loader and the previously saved sample:

- Derived ID `0x0b000011`: `dt0b.img` entry 17 at offset 5,992,448, length 81,872;
  decoded length 748,000. SHA-256
  `e5158b512d357b4329648bef9485f1ad859f7b510afb12cfc68d540a1c1e6c92`.
- Derived ID `0x0e000001`: `dt0e.img` entry 1 at offset 2,048, length 3,928,288;
  decoded length 3,928,272. SHA-256
  `7b2bb68d7efc91adf7ebcd5d90833bf36a73b0a80afd4b5b24d5c72d20f22d8c`.

Entry metadata agrees too, and archive size/timestamps remained unchanged.
The probe, comparison script, outputs and results stay under ignored
`private/research/engine/native-resource-01/`. The unchanged Python suite's
latest result remains 434 passing; it was not rerun for this C++ addition.

## Android build

Using the existing JDK, SDK and repository-local Gradle cache:

```bash
export GRADLE_USER_HOME="$PWD/private/gradle"
./android/gradlew -p android --offline --no-daemon :app:assembleDebug
```

The offline build succeeds with installed dependencies. Gradle needed permission
to open its local daemon socket outside the execution sandbox. Existing SDK XML
version warnings remain nonfatal. The diagnostic APK is 4,190,262 bytes, SHA-256
`745a282857631f861253c6ff83e3b691ad613e5bd5893966bbeb45810cadea00`.
APK signature verification passes v1 and v2; it reports the existing unsigned
Gradle metadata-entry warning. The three packaged libraries are ARM64 app/SDL/C++
libraries, with no packaged assets. `libmain.so` is AArch64 and exports both new
loader functions. The installed APK's hash matches this build.

## Note20 loading verification

The diagnostic in `native/app/main.cpp` runs on keyboard L, including ADB keycode
40. It reads the two derived sample IDs from `files/resource-img`, then writes
decoded files and `results.csv` into a fresh `files/resource-check` directory.
The CSV records per-resource status, entry count/offset/length, decoded length,
CRC32 and output name. Errors are logged and do not terminate the viewer.
`Resource check complete` means the diagnostic finished; check each status and
the loaded count to determine success. Reports are published by renaming a
completed temporary file. Existing result directories and output files are
refused. A write failure can leave partial files; do not use failed outputs.

The SM-N981U loaded separate copies of the checked archives from app storage.
Both decoded files were pulled and compared byte-for-byte with the verified
Python loader. Lengths and SHA-256 hashes match the host results above. Source
entry metadata agrees too. The device reports CRC32 `938f6f56` for the ball and
`929573eb` for the animation bundle.

Device cases pass:

- Valid archives: both entries load and all decoded bytes/metadata agree.
- Repeated L with existing results: the check is refused; the CSV and both
  decoded file hashes remain unchanged.
- Missing archive directory: both entries report errors and create no decoded
  outputs. The viewer process remains running.
- A synthetic AFS entry with a damaged zlib checksum in the animation archive's
  place: the ball still loads correctly; the animation reports an error with
  no decoded output. The original copy is retained separately during the test.
- Restored copies: both entries load and agree again. Their full archive hashes
  match the host inputs. All six pre-existing viewer/benchmark files are unchanged.

The diagnostic pauses playback and cancels an active benchmark. The existing
animation frame is still submitted; after the checks, Space resumes playback
from sample 60 and it reaches sample 90 normally. The same viewer process
survives every case. This is loading/control evidence, not a new visual review
or a game-performance measurement.

The comparison script, reports, decoded files and process-filtered logs remain
under ignored `private/device/resource-loader-01/`. Final successful results
remain at `files/resource-check/results.csv` on the device; known-good archive
copies remain in `files/resource-img`. No new packages or game captures were
needed. Only the Android adapter changed in this step: no Python or host-native
tests were added or rerun. The existing host results above remain applicable to
the unchanged loader. Device reproduction steps are in
[ANDROID.md](../ANDROID.md#check-native-resource-loading).

No original PES routine was executed, and the sample IDs were not observed as
historical call arguments. These comparisons establish agreement with the
checked Python implementation and saved outputs, not full equivalence to the
original engine routine. See
[the original loader evidence](../research/pes2013-engine.md#resource-ids-archive-entries-and-the-decode-stage).
