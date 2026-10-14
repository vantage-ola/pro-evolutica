# Investigate the original PES engine

The goal is to carry the original PES 2013 game behaviour to Android through
compatibility, translation or an evidenced native/hybrid implementation.
The asset viewer supports that investigation. It is not the start of a separate
football game with invented physics, AI or rules.

Static ball rendering and one dense skeletal clip work on the Note20. In the
other track, the original PES 2013 game launches and loads a match in Winlator;
the user reports a 60 FPS peak with persistent choppiness, and doubled or
reverberant menu/crowd effects while music and commentary sound normal. That is
playable evidence, not yet a reproducible performance result.

Reverse-engineering has moved from archive inventory to a specific successful
payload read. The Windows trace records `dt0b.img` at offset 5,992,448 for
81,920 bytes, with PES return address `0x004f7a0e`. This aligns with AFS entry
17's 81,872-byte WESYS object and the padded span to the next entry. The same
address is the live `ReadFile` boundary observed under CrossOver for a 2,048-byte
header read. Its successful path advances the reader and returns `1`; the error
path, larger live read, and full containing routine remain unobserved. The
existing decoded entry and Android ball viewer help validate the asset side of
the chain, but they do not implement PES's engine.

The Windows executable and archive hashes match the supplied Mac/CrossOver
copies. Ghidra and REA are available, but default analysis finds no containing
procedure at the captured PES addresses, and runtime call-site bytes differ
from the on-disk copy. See [the executable and runtime evidence](research/pes2013-engine.md).

Next capture the naturally occurring 81,920-byte read under WineDbg, correlate
its handle with `dt0b.img`, and inspect its return through the shared read
boundary. Then trace how that WESYS entry reaches the decoder/model loader and
identify one original function with enough boundary and behavior evidence to
compare against our existing parser. Use that comparison to choose between
improving compatibility and building a small native/hybrid component; do not
treat the viewer as a PES engine port.
See [the stack evidence](research/pes2013-engine.md#captured-file-io-stack-addresses).
Runtime observations establish platform boundaries and loading order, but do
not recover function instructions by themselves. Android verification follows
an implemented component; runtime research can proceed independently.

Advance after verified results or an explicit blocker and revised approach.
Keep the working Winlator installation available for the original game. The
completed asset work and its checks are retained below as supporting evidence.

## What already works

- The inspection tools identify executable architecture, imports and asset containers.
- AFS extraction and the observed WESYS compression variant pass synthetic tests
  and real-file checks.
- One DXT1 texture has been extracted, independently checked and shown on the
  native cube on the Note20. The image stays outside the APK and Git.
- The ARM64 app builds offline with the installed dependencies. Rendering,
  touch, PS4 input and background/resume have device evidence.
- One static ball model now renders with its own colour PNG on the Note20,
  with checked source geometry, visual reference agreement and file fallbacks.

These results establish an asset-loading and rendering path for one static
model. General model support, gameplay systems and the cause of
Winlator's stutter remain unverified. Animation support is limited to the checked
rigid model and dense clip.
See [STATUS.md](STATUS.md) for current evidence and limitations.

## Find and document one model

Inspect a small set of entries from the actual archives. Record the archive and
entry index, hashes, wrapper layout and model version. Check existing model
research against those bytes before choosing fields to support. Use a small
static sample first; a player with bones and animation can follow later.

Deliver a format note and a bounded read-only inspector. Tests use original
fixtures and cover truncated headers, invalid counts, offsets, unsupported
versions and output overwrite protection. Unknown fields stay labelled unknown.

Complete when extraction matches an independent source read, model boundaries
and table ranges are validated, and a repeatable command produces its metadata.
A signature match alone is a candidate, not a validated mesh.

## Validate its geometry and texture relationship

Read the supported vertex layout and indices. Establish coordinate axes, scale,
face winding and how indices form triangles. Check finite positions, index
ranges, vertex counts, bounds and degenerate triangles. Confirm the material's
texture reference against the sample's bundle. Check stored model/packet
transforms before treating local vertex coordinates as world coordinates;
proximity to another texture does not prove a match.

Deliver one local geometry output and its associated texture, plus original
fixtures with known vertices and faces. Compare counts and geometry with an
independent reader or model viewer, after checking that tool's licence and exact
format support. Document any disagreement before relying on the output.

Complete when the sample has plausible, checked geometry and an evidenced
texture mapping. If the simplest sample is a flat quad, use it to validate the
format, then choose a spatial model for the Android demonstration.

Current result: the spatial sample is a ball. Its geometry and texture-reference
metadata match a pinned independent reader; a checked local OBJ and DXT1 colour
PNG exist. IDs and visual inspection support the sample's texture selection.
The general bundle ID convention remains an inference, so selection stays
explicit. The Android result below verifies the selected texture's UV alignment.
See [the research record](research/pes2013.md#static-ball-geometry-and-textures).

## Display the model on Android

Replace the cube's fixed geometry with the supported sample. Keep the file
reader separate from SDL and OpenGL. Choose a runtime format based on the
validated sample and available licensed tooling; avoid a general asset framework.
Retain an original test scene when no local model is supplied.

Deliver a debug APK and commands to supply the local model and texture through
the app's storage. Verify orientation, texture coordinates, depth, face winding
and bounds on the Note20. Test missing and damaged files, background/resume,
and touch/controller input. Save screenshots and process-filtered logs privately.

Complete when the phone shows the same supported geometry and mapping as the
independent reference. A successful build or frame-submission log is insufficient.

Current result: the static ball and its colour texture render on the Note20.
Native vertex/index bytes match the independent reader output, and a separate
CPU render agrees visually with the device's silhouette and texture placement.
Missing/damaged file fallbacks, touch/PS4 controls, pause/resume, lifecycle and
Back pass. Three native CTest targets and sanitizer runs pass. See
[the device verification](verification/android.md#static-ball-model). Proceed to
camera controls and measurements; do not infer general model/animation support.

## Make the model inspectable and measure it

Add orbit, zoom and reset controls around the model's actual bounds. Verify
portrait/landscape framing and touch/controller behaviour. Measure load time,
frame times and memory for that scene with a stated method and repeatable run.
Report spikes and failures as well as successful results.

Complete when the model can be inspected from different angles without losing
it and the measurements identify the tested APK, device and input files.
Additional materials and scene objects follow only when supported samples exist.

Current result: orbit/zoom/reset are implemented and verified through native
camera/gesture tests and touch/PS4 device checks. Camera bounds keep the ball
visible at both zoom limits. See [the camera record](verification/android.md#camera-controls).
Load/frame-time/memory measurements now pass: the final portrait APK records
60.039 FPS over 60.011 seconds, p99 19.670 ms and a maximum interval of 25.509 ms.
Startup stages, memory/thermal snapshots, raw-sample recomputation and failed
capture evidence are recorded in [the measurements](verification/android.md#native-measurements).
This completes the static viewer checks; proceed to animation sample research.
It does not establish general model support or game performance.

## Investigate animation

Choose a model with an evidenced skeleton and one animation clip. Document bone
hierarchy, bind pose, weights and timing before implementing deformation.
Start with original tiny skeletal fixtures, then compare one real pose and a
short sequence with an independent reference. Do not infer gameplay meaning
from clip names or assume the unverified Squirrel scripts are usable.

Complete when the model's supported pose and motion agree with the reference.
Only then assemble a small scene using supported objects and animation.

Current result: the selected 19-bone model and dense clip have matching parent
links. Bone records, raw weights/palettes, 91 samples, 30 FPS sequence timing and
20 payloads agree with pinned readers. Original inspection fixtures pass.
See [the pair and remaining unknowns](research/pes2013.md#skeleton-and-dense-animation-samples).
The 41 original pose tests now pass. All packed rotations match the pinned reader;
one real pose agrees with a separate vector/quaternion calculation within
1.057e-6 source units. Local rotations, retained rest translations and
residual-first weights produce a coherent pose for this pair. Residual-last
badly stretches the geometry. See [the pose record](research/pes2013.md#one-evaluated-pose).
This is a source-based calculation and visual diagnostic, not an in-game or
Blender runtime comparison.

All 91 integer samples now agree with the separate calculation within 1.234e-6
source units. Adjacent movement, sampled bounds and duplicate bindings are
checked; no faces collapse. The faster opening frames match the reference.
First and last poses differ substantially, so the selected clip is not approved
for automatic looping. See [the full sequence record](research/pes2013.md#full-dense-sequence).
The portable native module now passes four focused original check groups and a
comparison of all real sampled vertices: maximum difference from Python is
1.351e-15, and from the separate reference is 1.234e-6 source units. Six native
targets pass normally and with sanitizers; Android ARM64/API 23 compilation
passes. See [the native record](verification/native-animation.md).
Bounded local loading and elapsed-time selection now pass focused checks.
All package-loaded real poses exactly match the verified native output. The
91-sample clip plays on the Note20, holds its endpoint and passes pause/resume,
replay, background exclusion and file fallback checks. The user confirms coherent
motion and PS4 controls. See [the playback record](verification/native-playback.md).
Interpolation, looping, materials, events and game-action meaning remain
separate questions. Proceed to original engine tracing below.

## Trace one original engine path

First establish a readable view of the original code and a working analysis
tool. Fingerprint an untouched executable separately if one is available;
addresses and results from the current copy must not be assumed to match it.
The current file's import slots and archive-directory records are starting
anchors, not function names. Do not turn arbitrary bytes into a claimed call graph.

Trace an actual consumer of the archive records or a verified file-I/O caller.
Record the input hash, function address, instructions, arguments, constants and
callers. Follow one bounded path before broadening the map to graphics setup,
timing and the update loop. A native equivalent requires input/output comparisons
with original behaviour; passing our own fixtures alone is insufficient.

Complete when one original function or small subsystem has an evidenced
behaviour description and a repeatable comparison. If static code is unavailable,
state the blocker and design a bounded observation of the legitimately running
game. Do not infer or bypass its protection mechanism.

## Choose a porting route from engine evidence

Keep the original game logic as the target. Evaluate whether the observed
Windows/x86 boundaries can remain under compatibility, be translated, or be
replaced incrementally by verified native components. Distinguish an executable
that runs through translation from a native ARM64 implementation.

Rendering extracted objects does not establish support for simulation, scripts,
physics, AI, animation transitions, audio, rules, menus or persistence. Investigate
these systems through original code and runtime evidence; do not invent their
behaviour to fill the gaps. There is no supported estimate for a full native port.
An original football simulation would require a separate explicit scope choice.

## Working rules

Use the existing branch and small commits. Keep proprietary files, converted
models and device captures under ignored local storage. Keep public documents
to technical findings and reproducible commands. Reuse installed dependencies;
ask before installing new packages. Update the status after each verified result.
Keep PES 2015 and Winning Eleven investigations separate until their formats
are inspected. Further Winlator tuning is optional and can proceed independently.
