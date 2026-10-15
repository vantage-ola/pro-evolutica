# Native static KTMDL reader

`native/assets/ktmdl.*` decodes the checked static model into the existing native
mesh type. It retains positions, normals, UV0, ordered triangles and numeric
texture references without an OBJ conversion or debug filenames. It adds no
dependency. The module compiles into the Android library; the viewer does not
call it yet.

## Scope

Supported input is standalone little-endian KTMDL 2.1/2.2, one triangle-strip
packet, one vertex stream and one uint16 index stream. Positions/normals must
be float3 and UV0 float2. No bones or one identity root are accepted. Source
coordinates and winding are retained. Strip restarts reset parity, and repeated
indices consume parity without producing a triangle.

Checks cover declared size, table/data bounds and overlap, relative offsets,
stream declarations, finite attributes, normals, triangle area, indices and
material references. Limits are 64 MiB input, 4,096 records per table, 65,535
vertices, 200,000 source indices and 600,000 output indices. Unsupported skinning,
morphs, non-identity transforms and stream layouts raise an error.

Texture metadata preserves both 64-bit halves of each ID, binding/name indices,
mode, parameter byte, UV channel and sixteen binding parameters. The material
index is read as a full 32-bit source field before validation. Additional source
UV sets are validated when referenced, but only UV0 enters the returned mesh.
The ball's normal reference uses UV1; normal/specular rendering, tangents,
samplers, shaders and automatic bundle pairing remain outside this reader.

## Verification

Using the existing CMake installation from the repository root:

```sh
cmake -S native -B private/native-tests
cmake --build private/native-tests
ctest --test-dir private/native-tests --output-on-failure
cmake -S native -B private/native-sanitizers \
  -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build private/native-sanitizers
ctest --test-dir private/native-sanitizers --output-on-failure
./android/gradlew -p android --offline --no-daemon :app:assembleDebug
```

All ten native targets pass normally and with AddressSanitizer/UBSan. The new
target has 69 checks using original synthetic data: geometry, strip parity and
restarts, full-width IDs, optional debug data, truncated regions, invalid
references/counts/offsets, non-finite values and unsupported layouts/transforms.

A private C++ probe decodes the model from entry 0 of resource `0x0b000011`.
Its 682 vertices and 1,144 triangles match every float32 vertex and uint16 index
byte from Python and a fresh run of the pinned independent reader. All returned
material fields also match both readers. Three synthetic Python fixtures cover
versions 2.1/2.2 and identity-root/material metadata. Normal and sanitizer probe
outputs agree for all four inputs.

The independent source is pinned at commit
`0a0284fc9127beca45397ba7009883af2f2be6fd`, SHA-256
`6dc2e924e9df136d72476343a0fd27feccac099fcdeb317ec9bcc49522d8ca63`.
Its source stays private and is not linked into the native reader. Ball hashes:

- Standalone KTMDL: `c93c7430f5534093fef4499dd5c7d3e4be5b530613558ac683d3bf6122e22aa3`.
- Vertex buffer: `01932e8cd850ad565123ec4651c23aef5ce39e0a70d8d0144b03a396aebcf7aa`.
- Triangle index buffer: `984c4558d4d846db67734f028daac75942ddccb40decd2a181ca36c97262f537`.

The three numeric references agree with the source metadata already compared
with captured model copies and drawable overrides in
[the original ball binding investigation](../research/pes2013-engine.md#original-ball-bundle-and-texture-binding).
These comparisons do not execute the original KTMDL routine or establish its
full behaviour.

The offline ARM64 build passes with the existing SDK XML warnings. Its APK is
4,261,702 bytes, SHA-256
`edc6b30ab1ca3e1cd29cf16aa09add3ee0268eff133dd2699299ac071bd8a51a`.
This verifies compilation; device integration follows separately. The next step
is to replace the T-key preview's OBJ input using the traced ball assignment,
while preserving the previous scene on failure.

Probe source, inputs, outputs and the comparison script/report remain ignored
under `private/research/engine/native-ktmdl-01/`. Game assets are excluded from
the APK and Git.
