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

The supplied installation copy is identical. A CrossOver file/DLL capture now
passes the checks below. The next capture is a Windows 11 reference run supplied
by the user, seeking archive read offsets and operation stacks. The prepared
Winlator procedure remains an alternative.

Capture a short, clearly labelled run from launch to the menu, then one match
load, using supported logging if available. Keep logs private and distinguish
observed filenames/API arguments from inferred engine roles. Logging can
identify platform boundaries and loading order; it cannot supply original
function instructions or justify a gameplay rewrite. State the chosen logging
method, limits and effect on performance before capturing it.

### CrossOver launch and capture

Local application metadata identifies CrossOver **26.2**, build
`26.2.0.39821`, with a bottle named `PES 2013`. The executable installed inside
that bottle is 19,916,288 bytes with the same SHA-256 as the inspected copies.
An independent `shasum` agrees, and the complete reconnaissance reports compare
equal. Installation has not provided a different static code view.

The user confirms **menu and match loading succeed, followed by a normal exit**,
using the **Wine** Graphics setting. These are manual results. Mac frame times,
audio behaviour and sustained performance have not been measured.

For the first capture:

1. Close PES. Select its launcher inside the `PES 2013` bottle and choose
   **Run with Options**.
2. Enable **Create log file**. Enter `+file,+loaddll` in **Wine logging channels**.
3. Choose a new `.cxlog` output under ignored
   `private/research/engine/crossover-capture-01/`. That directory is prepared.
4. Launch to the menu, load one match, and close PES after kickoff. Record which
   steps succeeded and any error. Close the Run with Options dialog as well.
5. Preserve the completed log before another capture. Launch normally afterward
   to avoid keeping this diagnostic logging enabled.

This workflow is documented in
[CodeWeavers' debug-log guide](https://support.codeweavers.com/2-creating-a-debug-log).
Use a new directory for each run. Logging can affect timing and generate large
output; this short diagnostic is not a performance measurement. First inspect
the actual log format and confirm PES file activity before implementing a
parser or drawing loading conclusions. The first capture is verified below.

Mac logs can establish observed archive access and compatibility calls. They
cannot establish Android rendering/performance or supply missing engine
instructions. Device verification follows an implemented component; it is not
required to collect this Mac research log.

### Verified CrossOver capture

The completed log contains 35,317,436 bytes and 367,138 lines, SHA-256
`77ffdee51d290783f013300816718bc588534f872eec8bf4c9b1a2bf351fec39`.
A bounded regular-file snapshot was checked for changes during reading; an
independent hash confirms the preserved copy matches. Raw logs and the detailed
summary remain in ignored `private/research/engine/crossover-capture-01/`.

Line 15,954 loads `pes2013.exe` at `0x00400000` under Wine process ID `0110`.
Counts below include that Wine process only, excluding startup/service processes.
The ID is not a macOS PID. Its trace spans 427.060 seconds; there are no stage
markers or frame samples, so this duration is not a gameplay benchmark.

- 66 `CreateFileW` requests open 16 distinct `dt*.img` archives. Each paired
  return supplies a non-invalid handle. First requests occur in this order:
  `dt06`, `dt03`, `dt01`, `dt02`, `dt04`, `dt07`, `dt08`, `dt09`, `dt0a`,
  `dt0b`, `dt0c`, `dt0d`, `dt0e`, `dt10`, `dt11`, `dt0f`.
- 2,956 `ReadFile` requests and 2,963 `NtReadFile` requests are recorded, with
  2,956 explicit `NtReadFile = SUCCESS` results. These API layers overlap and
  must not be added as distinct reads. Missing result lines do not prove failure.
- 65 module-load events include Wine's built-in `d3d9.dll`, `wined3d.dll`,
  `opengl32.dll`, `DSOUND.dll`, `DINPUT8.dll`, `XINPUT9_1_0.dll` and
  `winecoreaudio.drv`. This supports the reported Wine graphics setting.
  Repeated loads are events, not necessarily distinct modules. Loader terminology
  `native` describes Windows PE modules, not Android ARM64 code.

One narrow sequence is directly visible: line 38,260 requests `dt0b.img`, line
38,266 returns handle `0x294`, and lines 38,269–38,271 request and successfully
read 2,048 bytes. The installed archive passes the existing AFS inspector:
11,028,480 bytes, 37 slots, one empty, SHA-256
`e77dddd5fb9df4362bf71ec16530ed6af4af748f9a9eba1ee73b71e1ebd13ad1`.
Its hash and complete entry metadata match the earlier ball-archive report.

The trace does not reveal the seek buffer's file-position value. The null offset
pointer in `NtReadFile` is not evidence of offset zero. Handles are reused across
archives and close events are absent, so later reads on `0x294` cannot all be
attributed to the ball. No particular AFS entry, displayed asset or original
caller function is established by this sequence.

The game process has one `err` event: line 23,099 reports
`GL_INVALID_FRAMEBUFFER_OPERATION (0x506)` from `glClear`, before the first archive
open. Its impact is unknown; the user confirms subsequent menu/match loading and
normal exit. Line 366,732 records `LdrShutdownProcess`, consistent with that exit.
This error does not establish a crash or explain the separate Winlator stutter.
Wine warnings, handled exceptions and compatibility calls are not recovered
engine instructions. No original function has been ported.

### Windows reference capture, including an offline PC

The user has a Windows 11 gaming PC. It need not be online or remotely connected
to collect a local trace. Download the official
[Process Monitor ZIP](https://learn.microsoft.com/en-us/sysinternals/downloads/procmon)
on a connected machine and transfer it by USB. The checked release is 4.11;
its supported client is Windows 11 or newer. It records operation parameters and
thread stacks, and PML preserves the native event data for later inspection.

For the first run, capture launch → main menu → one exhibition match → normal
exit. A broader tour of game modes can follow once this trace is usable.

1. Extract the ZIP. On an x64 PC, run `Procmon64.exe` as administrator. Accept
   the licence if prompted. Stop the initial capture using **File → Capture Events**.
2. Reset existing filters, then add **Process Name → is → pes2013.exe → Include**.
   Clear the initial events with **Edit → Clear Display**. Keep file-system and
   process/thread activity enabled so archive reads and module loads are available.
3. Start capture, launch PES, reach its menu and load a match. Play briefly, then
   exit normally and stop capture. Note approximate clock times for menu, kickoff
   and exit, teams/stadium selected, graphics settings, and any visible problems.
4. Use **File → Save** and select **Events displayed using current filter**.
   Save `pes2013-pc-01.pml` in native PML format. Save the same filtered events
   again as `pes2013-pc-01.csv`; keep the **Detail** column in the CSV.
5. In PowerShell opened in the actual PES installation directory, fingerprint
   the executable without changing it:

   ```powershell
   Get-FileHash -LiteralPath '.\pes2013.exe' -Algorithm SHA256 |
       Format-List | Out-File -Encoding utf8 '.\pes2013-exe-sha256.txt'
   ```

6. Transfer the PML, CSV, hash text and a short `notes.txt` by USB. Retain the
   original capture on the PC. Full logs belong in ignored `private/` on the Mac.

Microsoft documents [capture, elevation and saving native logs](https://learn.microsoft.com/en-us/troubleshoot/windows-client/shell-experience/troubleshoot-apps-start-failure-use-process-monitor).
This project deliberately exports only the filtered PES events. Capture overhead
means this run is not a performance benchmark. CSV enables analysis with the
existing Mac tools; retain PML for richer event/stack inspection on Windows.
An offline stack may contain module addresses without resolved symbols.

Compare the PC executable hash before reusing Mac addresses. Inspect observed
read offsets/lengths and available stacks for one archive path first. File/API
logs cannot provide all simulation, AI or rendering behaviour; subsequent
captures should answer a specific unresolved question. The Windows procedure
is prepared, not yet tested on the user's PC.

### Prepare a short Wine log

Wine handles the game's Windows API calls. Its file-access and DLL-loading
debug channels can expose some of those calls while the original game runs.
This is an observation at the compatibility boundary, not an instruction trace
of the PES engine. Coverage depends on the Wine build and selected channels.

The current upstream Winlator source provides the following controls; confirm
that the installed version has them before treating these steps as verified:

1. Exit the running container. Record the Winlator version and existing logging
   settings so they can be restored afterward.
2. Open Winlator's main **Settings**. Enable **Wine debug**, and add **file** and
   **loaddll** to the Wine debug channels. Leave Box64 logging off for this run.
3. Enable **Save logs to file** and record the displayed output path. Save the
   settings, then restart the container so Wine receives the new channels.
4. Launch PES, reach its menu, load one match and stop after kickoff. Keep the
   run short; if loading or logging becomes unusable, exit and report that result.
   Note the actions and any errors. This diagnostic run is not an FPS benchmark.
5. Exit the container and copy the log before launching another session. Restore
   the previous logging settings when finished.

Upstream's default output is `Documents/Winlator/logs.txt`; use the path actually
shown by the installed build. Its log writer replaces the previous file when a
new log view is created, so preserve each capture before another launch.
These settings and behaviour are supported by the upstream
[settings implementation](https://github.com/brunodev85/winlator-app/blob/main/app/src/main/java/com/winlator/SettingsFragment.java),
[channel selection](https://github.com/brunodev85/winlator-app/blob/main/app/src/main/assets/wine_debug_channels.json),
[Wine environment setup](https://github.com/brunodev85/winlator-app/blob/main/app/src/main/java/com/winlator/XServerDisplayActivity.java)
and [log writer](https://github.com/brunodev85/winlator-app/blob/main/app/src/main/java/com/winlator/widget/LogView.java).

With USB debugging authorised, copy the completed default-path log to a new
ignored research directory on the Mac:

```bash
mkdir private/research/engine/winlator-capture-01
adb pull /sdcard/Documents/Winlator/logs.txt private/research/engine/winlator-capture-01/logs.txt
shasum -a 256 private/research/engine/winlator-capture-01/logs.txt
```

Choose a new directory for each capture. If the displayed phone path differs,
substitute it in `adb pull`. Keep full logs private. First inspect the captured
format and confirm that PES file operations are present before writing a log
parser. A successful pull alone does not verify tracing. Identify observed
archive opens, read/seek operations and loaded modules where the log supports
them; leave internal gameplay roles and callers unknown.

No Winlator runtime capture has passed yet. The latest ADB connection check lists no
device; the user has been given the capture steps. No logging setting or game
file has been changed by the agent.

If another executable becomes available, fingerprint it independently before
assuming readable code or matching addresses. Once a readable code view and
working analysis tool exist, trace an archive-record consumer or file-I/O caller
and follow its callers. Select one small function only after its instructions,
inputs and outputs are evidenced. Compare a portable equivalent with original
behaviour before calling it a port. Preserve the working Winlator installation.
