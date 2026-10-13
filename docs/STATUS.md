# Project status

PES 2013 runs in Winlator: peak 60 FPS, with choppy gameplay throughout the match.
Repeated menu effects remain unresolved. The separate native viewer now renders
one checked PES 2013 ball with its colour texture. See [DECISION_001.md](DECISION_001.md)
for the development direction and [PLAN.md](PLAN.md) for the next results and
their verification checks.

## Verified deliverables

- Python project, isolated `.venv`, pinned test dependencies, and exclusions for
  games, private reports, extracted assets and build outputs.
- `tools/pe_inspect.py`: JSON fingerprint, PE architecture, entry point, sections
  and static imports, with input bounds and CLI errors.
- `tools/asset_inventory.py`: recursive JSON/CSV metadata and magic bytes, with
  symlink/special-file exclusion and explicit partial-scan errors.
- `tools/afs_inspect.py`: bounded AFS entry-table inspection, archive hashes and
  extraction of one raw entry by index, with an output checksum.
- `tools/wesys_unpack.py`: bounded zlib decoding of the observed WESYS variant,
  with exact length/checksum checks and input/output hashes.
- `tools/pes_texture.py`: bounds-checked bundle/WE00 inspection, byte-exact DXT1
  DDS extraction, mip metadata and a PNG preview of the largest level.
- `tools/ktmdl_inspect.py`: bounded geometry metadata for observed little-endian
  KTMDL 2.1/2.2 triangle-strip packets. Real samples from `dt0b.img` have checked
  geometry table ranges, finite positions and in-range indices.
- `tools/ktmdl_export.py`: one static packet exported to local OBJ with normals
  and UV0. The selected ball's six vertex attributes, indices, triangle order,
  identity bone and texture references match a pinned independent reader.
  All 1,144 triangles have positive area and agree with the vertex normals.
  Texture selection is supported by the IDs and visual checks of the three
  bundle images; the binding convention remains sample-specific. Android model
  rendering and the visual UV check now pass for this ball.
- The native triangle runs on the Note20, with screenshot/log evidence for
  rendering, touch, PS4 X-button input, background/resume and Android Back.
- The original cube remains the fallback scene, with a local PNG or checkerboard,
  perspective and depth testing. Its two existing CTest targets pass:
  five cube check groups and 167 PNG checks, including sanitizer runs. The ARM64
  APK builds offline. Signature/library checks,
  installation, rendered screenshots, touch/PS4 input and background/resume
  checks pass. See [ANDROID.md](ANDROID.md) and
  [verification/android.md](verification/android.md).
- The bounded portable PNG reader loads the verified 256 × 256 extracted texture
  from the app's private storage on the Note20. Its decoded pixels match the
  independent reference exactly. Missing/malformed-file fallback, touch input
  and background/resume pass. Game images remain outside the APK and Git.
- The bounded portable OBJ reader loads the checked static ball export: 682
  vertices and 1,144 triangles. Its vertex/index bytes agree exactly with the
  independent reader output on the Mac and through phone CRC checks. The model's
  own 512 × 512 colour PNG renders with coherent panel/marking alignment and
  agrees visually with a separate CPU reference render. Missing/damaged model
  and image fallbacks, touch/PS4 tint, rotation pause/resume, background/resume
  and Android Back pass. The mesh CTest target adds 1,496 checks; all three
  targets pass normally and with sanitizers. The APK builds offline and contains
  no supplied assets. See [verification/android.md](verification/android.md#static-ball-model).
- The viewer has bounded orbit, zoom and reset controls. Touch drag/pinch/hold
  and PS4 sticks/Triangle pass manual checks; ADB drag, hold/reset and zoom-limit
  screenshots pass. Portrait/landscape framing and controls pass on the phone.
  Camera movement pauses rotation, and X/Options retain their
  existing actions. Four native test targets pass normally and with sanitizers;
  the APK builds offline. See [the camera checks](verification/android.md#camera-controls).
- The viewer records bounded frame samples, startup stages and private CSV/JSON
  results, with a repeatable ADB collector. The final portrait build measures
  60.039 FPS over 60.011 seconds, p99 19.670 ms and a 25.509 ms maximum;
  main-to-first-swap is 104.585 ms. Before/after process PSS is 98.97/94.30 MiB.
  Five native targets pass normally and with sanitizers; the APK builds offline.
  Input, resize and background cancellation are checked, including a corrected
  SDL lifecycle event watch. This is one static scene, not game performance.
  Conditions, earlier runs and rejected captures are in
  [the measurement record](verification/android.md#native-measurements).
- Skeletal inspection now preserves the selected model's 19 bone records,
  34 palettes and all 6,736 vertices' raw weights/indices. A bounded dense ANM
  inspector checks a structurally matching 91-sample clip, its 30 FPS sequence,
  track payloads and raw events. Every reported bone/influence/payload agrees
  with pinned independent readers; archive/chunk hashes are checked. Original
  two-bone/three-sample fixtures add 77 tests. The inspectors preserve raw data;
  pose evaluation is checked separately below. See
  [the selected pair](research/pes2013.md#skeleton-and-dense-animation-samples).
- `tools/anm_pose.py` evaluates one dense sample using rigid bind matrices and
  an explicit residual-weight order. The 41 original tests check packing, joint
  rotation, parent/root movement, bind recovery and refusal of unsupported data.
  All 1,729 selected packed rotations match the pinned reader. Sample 45's 6,736
  posed vertices and 19 joints agree with an independent vector/quaternion
  calculation within 1.057e-6 source units. A private CPU diagnostic supports
  residual-first for this model; residual-last badly stretches the geometry.
  No Blender runtime or in-game pose was compared. Full sampled motion is
  checked separately below; Android animation remains unverified. See
  [the pose evidence](research/pes2013.md#one-evaluated-pose).
- `tools/anm_sequence.py` measures inclusive dense ranges, adjacent movement,
  bounds, triangle areas and duplicate-position/binding gaps. Nineteen original
  tests check known movement, endpoint differences, quaternion signs, malformed
  later samples, work limits and CLI/file errors. All 91 selected poses match
  the separate calculation: 612,976 vertex comparisons and 1,729 joint origins,
  maximum difference 1.234e-6 source units. No faces collapse; identical bindings
  remain coincident within 2.238e-16. Fast opening motion agrees with the reference.
  The first/last geometry differs by up to 1.149206, so automatic looping is
  unsupported. See [the sequence evidence](research/pes2013.md#full-dense-sequence).
- Android Gradle configuration passes offline with JDK 21 and cached build
  dependencies. The included Gradle 8.9 wrapper matches the official JAR hash.
- Synthetic PE fixtures generated inside tests; no game executable or asset is
  checked in. Tests cover PE32/PE32+, named/ordinal/delayed imports, malformed
  files, allocation limits, CLI errors and report overwrite protection. Inventory
  tests cover Unicode/CSV quoting, read errors and parent-symlink replacement.
- **401 tests pass** on Python 3.11.9. Commands and results are in
  [verification/tools.md](verification/tools.md). Usage is in [TOOLS.md](TOOLS.md).
- The phone gameplay report is recorded in
  [benchmarks/pes2013-baseline.md](benchmarks/pes2013-baseline.md). The reported
  60 FPS is a peak reading, not a measured sustained average.

## Local game observations

The inspected PES 2013 and PES 2015 executable reports agree with `file` on
**PE32, x86, 32-bit Windows**. Their SHA-256
values agree with independent `shasum -a 256` runs:

- PES 2013: `5b1dd4fc86b6e92696762caa0f92e4e59f026858131d1be6a0418d1d2b275694`.
  19,916,288 bytes; 19 imported DLL descriptors, 330 reported functions.
- PES 2015: `aaa537541c2bae0742016dfd3c0f4c908dff3e5fab750a006419ab54bc4434ea`.
  39,111,472 bytes; 24 imported DLL descriptors, 623 reported functions.

PES 2013 produced a `pefile` warning that section 6 has both write and execute
flags. The suggested packing explanation is a parser hypothesis, not a verified
finding about this executable. PES 2015 produced no parser warnings.

The PES 2013 inventory scanned 42 files, 6,206,731,485 bytes, with no read errors or
skipped entries. It observed 19 `.img` and 13 `.sfd` files, among other extensions.
All 19 `.img` files passed AFS index inspection: 55,593 slots, 1,273 empty.
One 1,840-byte entry from `dt10.img` was extracted and compared byte-for-byte with
an independent source read. All ten non-empty entries have now been decoded from
WESYS/zlib: 140,960 bytes total, with exact lengths and matching reference output.
The decoded data has markers consistent with Squirrel compiled scripts; its
instructions and game behaviour remain unverified. Details and hashes are in
[research/pes2013.md](research/pes2013.md). Full reports and decoded bytes remain
under ignored `private/`; no proprietary content is included in source.

`dt08.img` entry 2 has been extracted, decoded and inspected: a 256 × 256 DXT1
texture with nine mip levels. The DDS extraction matches the source bytes; its
PNG preview was visually checked and compared with macOS's independent decoder
(maximum RGB difference 1/255; alpha matches). This provides a local texture
sample for a viewer. Its in-game use remains unknown; model work is recorded below.

Mesh research has selected `dt0b.img` entry 35 (a four-vertex flat sample,
KTMDL 2.1) and entry 17 (a spatial sample with 682 vertices, KTMDL 2.2).
Their extracted model ranges match source bytes and independent SHA-256 checks.
The spatial sample has 1,748 indices and 1,144 triangles. Its debug names and
shader identify a ball, with colour, normal and specular references. A local
114,904-byte OBJ preserves the source coordinates; its vertex/normal/UV values
round-trip to the same float32 bytes. The selected 512 × 512 colour texture is
DXT1; the normal texture is DXT5 and remains outside our decoder's supported
subset. The pinned reference cannot decode the flat sample's debug-name layout.
The scope, mapping evidence and hashes are in [research/pes2013.md](research/pes2013.md).

## Phone result

- PES 2013 launches after a fresh installation. The earlier
  `rld.dll failed to initialize (E200)` error no longer blocks launch. The specific
  installation change responsible has not been isolated.
- Menus and gameplay work, with peak 60 FPS and choppiness throughout the match.
- Menu effects repeat or reverberate, sometimes more than twice; crowd audio may
  also be affected. Music and commentary sound normal. Switching to PulseAudio
  produced the same issue; the original driver was not recorded.
- Winlator version, resolution, graphics settings and driver/wrapper versions
  have not been captured. Average FPS, frame times and thermal behaviour have not
  been measured. Further benchmarking is optional for the current asset work.

## REA, Ghidra and device status

- REA and Ghidra were not found in the checked locations. Their versions and
  provider/client `doctor` checks remain unverified.
- ADB sees an authorised `SM_N981U` running Android 13/API 33. The native app
  reports Adreno 650/OpenGL ES 3.2 and a PS4 controller. Triangle and cube evidence
  is recorded in [verification/android.md](verification/android.md). The exact
  Winlator configuration remains pending.

## Next three tasks

1. Implement the verified subset in a portable native deformation module, with
   original fixtures and a repeatable comparison to the checked sampled poses.
2. Add a bounded local model/clip loading path and elapsed-time sample selection.
   Hold the final sample; interpolation and looping need separate evidence.
3. Verify the supported animation on Android before updating the native-engine
   feasibility decision. General gameplay remains out of scope.
