# Native bundle texture reader

`native/assets/texture.*` reads an explicitly selected texture from a decoded
PES bundle and returns the largest mip as top-row-first RGBA8. This removes the
need for a PNG conversion at that boundary. It uses the existing `assets::Image`
type and adds no dependency. The Android viewer calls it through the optional
T-key ball preview; normal startup keeps its existing asset choice.

## Original code evidence

The supplied Windows snapshots expose these static relationships:

- `0x011ca190` dispatches on input byte 7. Its two-stage jump table maps
  `0xff` to `0x011ca25a`, which calls constructor `0x0042c490` at `0x011ca288`.
  Other supported dispatch values are `8`, `16`, `24` and `32`.
- That constructor calls `0x011cb230`, which calls initializer `0x011caee0`.
  When input byte 6 is zero, the initializer allocates and copies
  `u32(input + 8) + 16` bytes, including the wrapper. It reads width from a
  16-bit field at input offset 32 and height from the low 16 bits of offset 28.
  These positions agree with the checked WE00-wrapped DDS samples.
- The named-file consumer `0x00cc0f90` compares a name's final four bytes with
  `.tex` and calls this factory at `0x00cc11f4`. Its bundle record's third word
  supplies the name offset. The checked ball and animation bundles instead
  contain `0xfffffff0` there. Their use of this named consumer is unproved;
  the native reader keeps selection numeric and leaves that word opaque.

Ghidra imported bounded captured memory windows at their original addresses.
The checker verifies all six windows (72 KiB) and 1,372 emitted instruction
locations against menu, kickoff and replay snapshots, plus the dispatch tables
and five encoded call targets. No original function was executed. The first
factory listing omitted switch-arm instructions from its reported body; each
table target was subsequently disassembled explicitly before checking calls.
Pseudocode remains a guide to the assembly, not a recovered C interface.

Evidence, the Ghidra project, private sample outputs and reproducible checker:
`private/research/engine/windows-capture-01/resource-consumer-01/`.
The executable identity is recorded in [the engine research](../research/pes2013-engine.md).

## Supported input and checks

The reader accepts the observed count/table header with 12-byte records,
WE00 type byte 0 and format byte `0xff`, then a flat DXT1 DDS. It validates the
whole entry table, exact DDS mip lengths and at most 15 zero padding bytes.
Limits are 64 MiB input, 4,096 entries, 4,096 per image dimension and 4,194,304
pixels. It refuses DXT5, cube/volume textures and unsupported headers.

The DXT1 implementation retains the existing Python decoder's rounding,
including transparent selector 3 when the first endpoint is less than or equal
to the second. Lower mips are length-checked; only the largest is decoded.
Material IDs, automatic texture binding, named bundles and the original
mip-selection policy are outside this reader. The preview uploads its output
through the existing RGBA renderer, which generates its own mips.

## Verification

Commands from the repository root, using the existing CMake installation:

```sh
cmake -S native -B private/native-tests
cmake --build private/native-tests
ctest --test-dir private/native-tests --output-on-failure
cmake -S native -B private/native-sanitizers \
  -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer'
cmake --build private/native-sanitizers
ctest --test-dir private/native-sanitizers --output-on-failure
./android/gradlew -p android --offline --no-daemon :app:assembleDebug
```

Both CTest runs pass all nine targets. The new target has 223 checks using
original synthetic data: explicit selection, interpolation rounding,
transparency, partial edge blocks, multiple mips, every truncated fixture
prefix, invalid unselected records, allocation limits and unsupported formats.

A private C++ probe calls `load_resource(..., 0x0b000011)` and then the texture
reader, without using a preconverted image. Ball bundle indices 1 and 3 each
produce a 512 × 512 RGBA image from ten-mip DDS inputs. Both images match every
Python output byte in normal and sanitizer builds. Colour RGBA SHA-256:
`e9065aa7ebf552bf592b1d84ec8b72a6a25931a5c63668ca315189fd834592ae`.
Specular RGBA SHA-256:
`14bc0c2964be590e516c957ad8c51a8780804a474b4d100ca2de8905d6ba129f`.

The saved independent macOS colour decode has identical alpha; 13,416 of
1,048,576 channels differ, with maximum error 1/255, reproducing the earlier
Python comparison. This is not pixel equality with the original game's GPU.
The model entry and DXT5 normal texture are explicitly rejected.

The offline Android build passes with the existing SDK XML warnings. The APK is
4,198,814 bytes, SHA-256
`14a7b6142f905505b7c7ac979528ba57350041ece97a3a1922c98755954f6f5e`.
That build established ARM64 compilation. Device integration follows below.

## Note20 preview

The T-key adapter loads the checked `model.obj` and the original archive's
colour texture into a temporary renderer, then swaps it into the app only when
both uploads succeed. It clears animation playback and resets the static
scene, paused. Failed reads or decoding leave the previous scene intact.
The default animation startup and the separate L-key diagnostic are retained.

The offline integration APK builds successfully and is installed on the Note20
SM_N981U. It is 4,201,430 bytes, SHA-256
`2dca018985e34f1aed4c6afad8146577618604176d5569f666fb48c7b26aaa28`;
the installed APK hash matches. The phone reports 512 × 512 RGBA8 with CRC32
`643dc8d1`, matching the host's decoded colour pixels, and 682 mesh vertices /
1,144 triangles. The saved screenshot shows the blue/yellow ball with coherent
panel and logo placement. This checks our renderer, not the original game's
rendering or material behaviour.

A matched paused PNG/archive pair in the same process has zero differing
channels across the 2,160 × 880 scene region (7,603,200 RGBA channels), excluding
Android bars and edge overlays. The first comparison was discarded because
the PNG scene had not received its reset event and was still rotating; some
early captures also changed orientation. The accepted pair waits for logged
startup/reset completion and uses the same landscape viewport for both paths.

Repeated T loads and missing archive/model inputs pass. A synthetic archive
with an empty selected slot is refused. The viewer process survives these
cases, preserves the scene and loads the restored archive again. Rotation,
camera movement and reset respond to ADB keyboard input. B refuses the preview
benchmark. These checks do not repeat the earlier manual PS4/touch review.

All six existing viewer/benchmark files, both archive copies and the three
existing L-diagnostic outputs have unchanged SHA-256 values after verification.
Temporary input moves were restored; the installed preview remains paused on
the ball. No new dependency or public asset was added. The portable readers
were unchanged, so their previously passing nine normal/sanitizer CTest targets
were not rerun for this Android adapter.

Private scripts, process-filtered logs, input hashes and screenshots are under
`private/device/archive-texture-01/`. Geometry still requires the OBJ export;
the next engine investigation is numeric bundle selection and material binding.
