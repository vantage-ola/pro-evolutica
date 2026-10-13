# PES 2013 executable reconnaissance

The next investigation follows the original executable. The asset viewer is a
format/rendering diagnostic; it is not a replacement football simulation.
No original engine function has been recovered or ported yet.

## Input and method

The inspected `pes2013.exe` remains 19,916,288 bytes, SHA-256
`5b1dd4fc86b6e92696762caa0f92e4e59f026858131d1be6a0418d1d2b275694`.
It is PE32/x86 with image base `0x00400000`. The current on-disk entry point is
`0x01c8c030`, inside `.SIAUJzi`, rather than the large `.text` section.
All addresses below are preferred virtual addresses; relocation at runtime can
change them. The input was read, hashed and parsed, without execution or modification.

`tools/pe_recon.py` maps selected imports to their IAT slots, scans executable
raw bytes for two indirect call/jump shapes, maps NUL-terminated ASCII anchors
and searches raw sections for literal pointers to them. These are byte
candidates, not instruction-decoded references or function boundaries. Missing
shapes do not prove that an API is unused: register-indirect calls, code
transformation and other instruction forms are outside this scan.

The new scanner uses the existing PE validator on the same immutable snapshot.
A separate DOS/PE section-table calculation using `struct`, without pefile's
address mapper, confirms all three selected ASCII VAs and literal pointer bytes.
Independent `shasum` agrees with the executable fingerprint after inspection.
Full reports remain in ignored `private/research/engine/`.

The user also supplied the executable from the copied Winlator installation.
Both files are 19,916,288 bytes with the SHA-256 above. An independent `cmp`
returns 0, and the complete reconnaissance reports agree. The copy therefore
does not provide a different static code view. Its installation history and
original-build provenance have not been established. Neither file was changed.

## Original file-I/O, graphics and timing anchors

The selected static imports and IAT virtual addresses are:

- `KERNEL32.dll!CreateFileA`: `0x013f819c`; `CreateFileW`: `0x013f81ec`.
- `KERNEL32.dll!ReadFile`: `0x013f8174`; `SetFilePointer`: `0x013f8198`.
- `d3d9.dll!Direct3DCreate9`: `0x013f8494`.
- `KERNEL32.dll!QueryPerformanceCounter`: `0x013f82b4`.
- `USER32.dll!PeekMessageA`: `0x013f83b8`.

For these seven slots, the executable sections contain no `FF 15 <slot VA>` or
`FF 25 <slot VA>` byte candidates. This is a negative result for those two shapes,
not a recovered call graph. The imports support investigation of Windows
file/graphics/timing boundaries; they do not identify a game update loop.

## Archive-directory data

Readable `.rdata` strings connect to adjacent 16-byte records in `.data`.
The observed records contain two string pointers followed by two integer words.
Their general count, consumer function and integer meanings are still unknown.

- Record `0x0169ddc0`: directory pointer `0x01514884`, archive pointer
  `0x015148bc` → `dt0b.img`. The directory ends in `various/ball/`.
- Record `0x0169ddd0`: directory pointer `0x015148c8`, archive pointer
  `0x01514900` → `dt0c.img`. The directory ends in `various/pl0/`.
- Record `0x0169dde0`: directory pointer `0x0151490c`, archive pointer
  `0x01514944` → `dt0d.img`. The directory ends in `various/pl1/`.
- Record `0x0169ddf0`: directory pointer `0x01514950`, archive pointer
  `0x01514988` → `dt0e.img`. The directory ends in `various/anime/`.
- Record `0x0169de10` similarly pairs `dt10.img` with `various/script/`.

The four selected ball/player/animation records end with literal words `5, 1`;
the script record ends with `0x25, 1`. The pointer/string relationships and
16-byte spacing are facts from the file. An archive/directory registry is the
working interpretation. Strings may be build-time paths rather than paths
opened in the installed game. This supports the previous asset selection but
does not identify the displayed character or prove that its hands are gloves.

## Static-code limitation

`.text` starts at VA `0x00401000` and contains 16,732,160 raw bytes. Its measured
Shannon entropy is `7.999989` bits per byte, near the maximum of eight. Together
with the unusual entry point and absence of the selected call shapes, this
suggests transformed/protected on-disk code. Entropy alone cannot identify an
algorithm or prove encryption/packing. The mechanism and runtime code are unknown.

Apple LLVM 21's PE disassembler rejects the unchanged input with
`debug directory has uneven size`. The debug-directory size is 172 bytes,
which is not a multiple of the 28-byte entry layout in the
[Microsoft PE specification](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#debug-directory-image-only).
That header issue and the `.text` interpretation are separate limitations.
No header patch, unpacking, decryption or protection bypass was attempted.

Ghidra and REA are not available in the checked locations. The installed JDK 21
is usable, but no decompiler health check or game decompilation has passed.
Official [Ghidra 12.1.4](https://github.com/NationalSecurityAgency/ghidra/releases/tag/Ghidra_12.1.4_build)
fits [REA's documented 12.1.x/JDK 21 requirements](https://rea.tools/get-started/#set-up-native-analysis).
Installing it still requires approval under the repository specification.
A decompiler alone will not establish meaningful instructions in transformed code.

## Next evidence required

The supplied installation copy is identical, so the next step is to establish
what file/API logging the installed Winlator build supports. Its version remains
unconfirmed: ADB found `com.winlator` for the primary user, but the device
disconnected before version metadata could be read. No game trace was captured
and no container settings were changed.

Capture a short, clearly labelled run from launch to the menu, then one match
load, using supported logging if available. Keep logs private and distinguish
observed filenames/API arguments from inferred engine roles. Logging can
identify platform boundaries and loading order; it cannot supply original
function instructions or justify a gameplay rewrite. State the chosen logging
method, limits and effect on performance before capturing it.

If another executable becomes available, fingerprint it independently before
assuming readable code or matching addresses. Once a readable code view and
working analysis tool exist, trace an archive-record consumer or file-I/O caller
and follow its callers. Select one small function only after its instructions,
inputs and outputs are evidenced. Compare a portable equivalent with original
behaviour before calling it a port. Preserve the working Winlator installation.
