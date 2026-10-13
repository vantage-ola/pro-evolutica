# Native PES asset research plan

The native app displays one verified PES 2013 ball with its own texture.
The viewer has a repeatable device measurement baseline. One 19-bone model and
a structurally matching dense clip now have checked metadata, poses and a full
sampled sequence. Portable C++ deformation, bounded local loading and timed playback now agree
with those results, and the clip plays on the Note20. The next target is a
native-engine feasibility decision and a small explicit gameplay scope. Each step ends
with a result we can inspect and repeat.
Advance only when its checks pass, or record a specific blocker and change the
approach. The working Winlator installation remains the route for playing PES.

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
separate questions. Proceed to the feasibility decision below.

## Decide what a native game would require

Use the viewer and animation results to write an engine feasibility decision.
List the systems still missing: player control, ball physics, collision,
animation transitions, rules, AI, cameras, audio, menus and persistence.
Executable research needs verified tooling and function-level evidence.

Choose an explicit next scope: deeper compatibility work, continued asset
research, or a small original football simulation. An original simulation needs
its own behaviour specification and tests; displaying PES assets does not
reproduce the proprietary engine. A complete native PES port has no supported
estimate at this point.

## Working rules

Use the existing branch and small commits. Keep proprietary files, converted
models and device captures under ignored local storage. Keep public documents
to technical findings and reproducible commands. Reuse installed dependencies;
ask before installing new packages. Update the status after each verified result.
Keep PES 2015 and Winning Eleven investigations separate until their formats
are inspected. Further Winlator tuning is optional and can proceed independently.
