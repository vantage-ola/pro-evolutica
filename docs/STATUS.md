# Project status

PES 2013 runs in Winlator: peak 60 FPS, with choppy gameplay throughout the match.
Repeated menu effects remain unresolved. The separate native viewer now renders
one checked PES 2013 ball with its colour texture and plays one verified dense
skeletal clip from local storage. See [DECISION_001.md](DECISION_001.md)
for the development direction and [PLAN.md](PLAN.md) for the next results and
their verification checks. The development target is the original game through
compatibility or an evidenced native/hybrid implementation; the viewer supports
that research, and a separate original football simulation is outside the current scope.

## Verified deliverables

- Python project, isolated `.venv`, pinned test dependencies, and exclusions for
  games, private reports, extracted assets and build outputs.
- `tools/pe_inspect.py`: JSON fingerprint, PE architecture, entry point, sections
  and static imports, with input bounds and CLI errors.
- `tools/asset_inventory.py`: recursive JSON/CSV metadata and magic bytes, with
  symlink/special-file exclusion and explicit partial-scan errors.
- `tools/afs_inspect.py`: bounded AFS entry-table inspection, archive hashes and
  extraction of one raw entry by index, with an output checksum.
- `tools/wesys_unpack.py`: bounded stored/zlib WESYS decoding, with exact length
  checks, zlib checksum validation and input/output hashes.
- `tools/pes_resource.py`: base-archive resource-ID loading for the checked
  `dt0b`–`dt0e` selectors, reusing AFS/WESYS validation without a whole-archive scan.
- `native/assets/resource.h` and `.cpp`: matching bounded C++ resource loading.
  Ball/animation bytes agree with Python and saved samples. All eight native
  targets pass normally and with sanitizers; the module builds into the ARM64
  APK. The Note20 diagnostic reproduces both outputs byte-for-byte and handles
  missing/corrupt inputs without terminating the viewer. See
  [the native loader checks](verification/native-resource.md).
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
  checked separately below; device playback is now verified in the native record. See
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
- `native/animation/skeleton.*` evaluates the verified rigid bone transforms and
  weighted positions in portable C++17. One focused native target covers four
  original check groups; all six CTest targets pass normally and with sanitizers.
  Every sampled real vertex agrees with Python within 1.351e-15 and with the
  separate reference within 1.234e-6 source units. Android ARM64/API 23 compilation
  passes. No new Python tests were needed for deformation. Local loading and
  device playback are checked separately below. See
  [the native verification](verification/native-animation.md).
- Android Gradle configuration passes offline with JDK 21 and cached build
  dependencies. The included Gradle 8.9 wrapper matches the official JAR hash.
- Synthetic PE fixtures generated inside tests; no game executable or asset is
  checked in. Tests cover PE32/PE32+, named/ordinal/delayed imports, malformed
  files, allocation limits, CLI errors and report overwrite protection. Inventory
  tests cover Unicode/CSV quoting, read errors and parent-symlink replacement.
- **407 tests pass** on Python 3.11.9. Earlier tool results are in
  [verification/tools.md](verification/tools.md), and the latest inspection result
  is in [verification/tools.md#executable-reconnaissance](verification/tools.md#executable-reconnaissance).
  Usage is in [TOOLS.md](TOOLS.md).
- The phone gameplay report is recorded in
  [benchmarks/pes2013-baseline.md](benchmarks/pes2013-baseline.md). The reported
  60 FPS is a peak reading, not a measured sustained average.
- `tools/animation_export.py` and `native/animation/clip.*` provide bounded local
  loading and elapsed-time dense playback. All 612,976 package-loaded positions
  exactly match the verified native bridge. Three focused Python tests and one
  native target cover format validation and timing; 404 Python tests and all
  seven native targets pass, including sanitizers. The ARM64 APK builds offline
  and plays the 91-sample clip on the Note20. Pause/resume, final hold, replay,
  background exclusion and missing/malformed fallback pass. The user confirms
  coherent motion and PS4 Options/Triangle; portrait/landscape screenshots are
  saved privately. Geometry uses simple white lighting. No game files enter the
  APK or Git. See [the playback record](verification/native-playback.md).
- `tools/pe_recon.py` maps selected x86 PE32 import slots, ASCII anchors,
  literal pointer candidates and section entropy without executing the game.
  Three focused synthetic tests cover address mapping, ambiguous byte matches,
  work/file limits and output protection. All 407 Python tests pass. Independent
  section-header calculations verify three real archive-name addresses. Original
  data records pair ball/player/animation archives with directory strings.
  This executable's large `.text` section has near-maximal byte entropy and no
  selected indirect-call/jump shapes; no engine function is claimed. Apple LLVM
  also refuses its malformed debug-directory size. The supplied Winlator
  installation copy is byte-for-byte identical, with matching reports.
  Readable original code is still required for function tracing. See
  [the executable record](research/pes2013-engine.md).

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

- Ghidra 12.1.4 and REA 6.4.0 are installed locally with a compiled ARM64
  decompiler. Ghidra provider and Codex registration health checks pass.
  A known synthetic x86 PE function decompiles correctly through the CLI and
  saved MCP launcher. REA tools are now available in this chat.
  See [the installation verification](verification/engine-tooling.md).
- ADB sees an authorised `SM_N981U` running Android 13/API 33. The native app
  reports Adreno 650/OpenGL ES 3.2 and a PS4 controller. Triangle and cube evidence
  is recorded in [verification/android.md](verification/android.md). The exact
  Winlator configuration remains pending.

## Mac runtime investigation

The user confirms menu and match loading, then normal exit, inside CrossOver
with the Wine Graphics setting.
Local metadata identifies CrossOver 26.2, build `26.2.0.39821`, and a `PES 2013`
bottle. Its installed executable has the same size, SHA-256 and reconnaissance
report as the previously inspected copies; independent `shasum` agrees.
The preserved file/DLL log is verified: 66 archive-open requests across 16
archives and 2,956 explicit successful read results in the PES Wine process.
The opened ball archive's hash/table match the previous research input. One
early WineD3D `glClear` error has unknown impact; it did not prevent the user's
reported menu/match run. File offsets and original caller functions are absent.
See [the capture findings](research/pes2013-engine.md#verified-crossover-capture).
No Mac performance measurement or original engine function is claimed.

## Windows runtime investigation

The user confirms menu, kickoff, a full match and exit from the game's menu.
The supplied CSV has 28,253 PES-only events, 19 successfully opened archives,
explicit read offsets/lengths and a process exit status of 0. One ball-archive
read matches the offset and padded length of the viewer's reference entry 17.
The supplied PowerShell hash photo confirms the PC archive and executable match
the inspected Mac inputs. The displayed ball choice remains unverified.
CSV/PML inputs were moved unchanged from the root into ignored `private/`.
CSV counts and the reference AFS table were cross-checked. A bounded local PML
inspection now matches all CSV timestamps and read ranges and extracts the
selected ball read's PES stack address `0x004f7a0e`. It appears across 5,063 archive
read events, suggesting a common file-reading boundary. Additional PES addresses
are captured in archive-open stacks; their functions remain unidentified. See
[the stack evidence](research/pes2013-engine.md#captured-file-io-stack-addresses).
Ghidra default analysis now finds no containing procedure at any of the four
captured PES addresses. Four 64-byte provider windows match independently mapped
source bytes exactly, and the source digest remains unchanged. The temporary
analysis session is closed. A subsequent launch under CrossOver's 64-bit
WineDbg captures one successful startup `ReadFile` call: requested 4,096 bytes,
returned 852, with a prefix matching local `settings.dat`. The call site at
`0x004e7b3b` uses the expected import and differs from its on-disk bytes.
File identity is correlated, not established by a live handle-name query.
Late attachment faults in this setup; launching under the debugger works.
Breakpoints were removed, the debugger detached, PES remained running and the
user confirmed it responded normally.
A second debugger launch directly connects `dt06.img`'s filename to a returned
handle, successful seek to zero, and successful 2,048-byte read through the
Windows trace's common frame `0x004f7a0e`. Its AFS prefix matches the local
archive, whose 1,442-entry table passes the existing inspector. Three runtime
API call sites differ from their on-disk bytes. All breakpoints were removed;
the user confirms normal response and exit, and PES is no longer running.
See [the Ghidra check](research/pes2013-engine.md#ghidra-check-of-the-captured-frames).
See [the runtime observation](research/pes2013-engine.md#runtime-file-read-observation).
See [the archive call chain](research/pes2013-engine.md#runtime-archive-open-seek-and-read).
The bounded WineDbg capture follows the successful `0x800`-byte read path:
it advances the reader cursor and returns `1` through an indirect callback.
Only the success path was observed; the failure branch and full routine entry
remain unresolved. See [the successful read return](research/pes2013-engine.md#observed-read-routine-return).
At the shared call site, the source executable contains undecodable bytes,
while WineDbg observed a `ReadFile` import call in CrossOver memory. The supplied
Windows menu/kickoff/replay minidumps now contain that same live instruction.
All three dumps pass header, directory and captured-range checks; independent
hashes agree. A second mapping check reproduces all nine bounded code windows,
which are identical across states. The captured IAT target independently resolves
to the 32-bit `kernel32.dll` export `ReadFile`.
Ghidra now disassembles/decompiles the original read worker at `0x004f7970`,
the open routine at `0x004f7740`, and size/seek helpers at `0x00505730` and
`0x00505780`. The read worker uses chunks of at most 128 KiB rounded to 2,048-byte
boundaries, consistent with the earlier padded archive read. These are file-I/O
findings; no gameplay engine function has been recovered or ported. The dumps
do not provide historical call arguments or prove the worker's error paths.
The x32dbg capture was interrupted; the user reports immediate recovery after
detachment. No additional device capture is currently needed. See
[the snapshot findings](research/pes2013-engine.md#verified-windows-memory-snapshots).
No gameplay engine function or Windows frame-time measurement is claimed.

The read worker's background dispatch is now traced through submission,
callback storage and invocation. A separate original helper at `0x01134d40`
reads the known WESYS length fields and passes the payload to a zlib-compatible
decoder at `0x01302ac0`. Ten bounded windows match across all three snapshots;
the existing ball decoder reproduces its saved output and all 33 focused tests
pass. See [the first loader findings](research/pes2013-engine.md#read-job-dispatch-and-wesys-compatible-decoding).

The higher-level resource loader now connects archive selection, entry streams,
the file scheduler and the decode stage through static calls, function tables
and shared buffer fields. Captured sizes and calculated offsets match every
entry in `dt0b`–`dt0e` across all three snapshots. A private base-archive check
uses derived resource ID `0x0b000011` to read ball entry 17 directly from the
archive and reproduce all 748,000 saved decoded bytes. This is not an execution
check of the original routine or evidence that this ID was passed during the
recorded match. See [the resource-loader route](research/pes2013-engine.md#resource-ids-archive-entries-and-the-decode-stage).

The public Python loader now reproduces the saved ball and stored animation
bundle directly from their archives using derived IDs `0x0b000011` and
`0x0e000001`. All output bytes agree: 748,000 and 3,928,272 bytes respectively.
The 89 focused tests and all 434 Python tests pass. Original-routine equivalence
remains unverified; the later C++ device check is recorded below. See
[the prototype checks](research/pes2013-engine.md#base-archive-loader-prototype).

The corresponding C++ module now passes the same two real-file comparisons in
normal and sanitizer builds. All eight native targets pass, including the new
resource target's 134 synthetic checks. The offline ARM64 APK build and signature
verification pass, with both loader functions present in `libmain.so`.

The viewer's L-key diagnostic now invokes that loader on the Note20 using
separate local archive copies. Both decoded outputs and entry metadata agree
exactly with the host results. Missing archives and a synthetic corrupt zlib
entry report errors without terminating the viewer; restoration succeeds.
Existing results are protected from overwriting, prior viewer/benchmark files
are unchanged, and animation playback still responds. The installed APK matches
the checked build. These buffers are not yet consumed by the renderer or game
systems. No new dependency or game capture was needed. See
[the native resource record](verification/native-resource.md#note20-loading-verification).

## Next tasks

1. Use the saved Windows snapshots to trace the original code that consumes
   decoded resources. Establish its bundle-entry selection and data layout.
2. Choose one small consumer with supported instructions and sample data, then
   implement and compare that behaviour before extending the Android path.
3. Verify the resulting native component on the phone. Keep original-routine
   execution comparisons and unsupported gameplay behaviour explicit; use the
   existing captures before requesting more data.
