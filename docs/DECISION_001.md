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
preview. Next, build the smallest original ARM64 rendering application with an
original test texture, then load this locally supplied PNG. Keep the game asset
outside the APK and Git. Meshes and the proprietary engine remain unsupported.
