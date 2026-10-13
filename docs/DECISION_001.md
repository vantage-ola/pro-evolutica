# Continue with asset readers

PES 2013 is playable through Winlator. The manual phone result is peak 60 FPS
with choppy gameplay throughout the match. PulseAudio did not resolve repeated
menu effects; music and commentary remain clear. This establishes compatibility
for the tested copy, but does not establish smooth sustained performance or the
cause of the stutter.

Continue original native research without requiring further Winlator benchmarks.
Keep the working compatibility setup available for gameplay and record later
tuning separately.

Start with the actual AFS containers. The new reader has inspected all 19 local
`.img` files and extracted one entry with a verified checksum. It uses no new
dependencies and makes no assumptions about what an entry represents.

The WESYS decoder now passes synthetic tests and decodes all ten non-empty
`dt10.img` entries. Their markers indicate likely compiled scripts, with VM
compatibility and behaviour still unverified. The texture reader now extracts a
verified 256 × 256 DXT1 DDS from `dt08.img` entry 2 and produces a checked PNG
preview. The original ARM64 app now renders a cube and loads this local PNG on
the Note20. The game asset remains outside the APK and Git.

Continue with one evidenced model sample and its texture. Document and validate
the geometry before adding Android model loading. Camera controls follow that
model result. Animation and engine feasibility have their own verification
checks in [PLAN.md](PLAN.md). A bounded inspector now checks geometry metadata
for two local KTMDL samples. Model rendering and the proprietary engine remain
unsupported until their respective results exist.

The spatial sample is now identified as a ball. Its static OBJ geometry matches
an independent reader exactly, including strip winding, normals and both UV
sets. The portable reader and Android renderer now display it with the selected
DXT1 colour texture. Source-byte checks and a separate visual reference agree;
fallback, input and lifecycle checks pass. Orbit/zoom/reset now pass touch,
PS4 and portrait/landscape checks. Native measurements now establish a short
static-scene baseline: 60.039 FPS, p99 19.670 ms and a 25.509 ms maximum interval
in the final portrait APK. See [the measured conditions and limitations](verification/android.md#native-measurements).
A selected 19-bone model and structurally matching 30 FPS clip now have checked
metadata. Original pose fixtures and one real-pose comparison now pass, with
residual-first supported for this sample. The full 91-sample sequence now agrees
with the separate calculation, including its faster opening motion. The clip
should hold at its final sample rather than automatically loop. Portable C++
deformation now passes the original checks and the full sampled comparison.
Continue with bounded local loading and sample timing before device playback.
See [the native results](verification/native-animation.md) and
[the pose evidence and limits](research/pes2013.md#one-evaluated-pose).
Normal/specular shading remains separate work.
