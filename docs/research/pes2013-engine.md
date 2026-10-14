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

Ghidra 12.1.4 and REA 6.4.0 are now installed with the existing JDK 21 and
bundled Node 24. The ARM64 decompiler, provider health check, Codex registration
and synthetic PE decompilation through the saved MCP launcher are verified.
See [the installation record](../verification/engine-tooling.md).
No PES decompilation has been performed during setup.
A decompiler alone will not establish meaningful instructions in transformed code.

## Next evidence required

The supplied installation copy is identical. CrossOver and Windows file-activity
captures now pass the checks below. The Windows trace supplies archive read
offsets, and its saved PML now supplies matched stack addresses. Next inspect
the corresponding original code with a working analysis provider. The supplied
PC hash photo now confirms both fingerprints match the inspected inputs.
The prepared Winlator procedure remains an alternative.

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
has produced the supplied CSV/PML capture below; the exact recorder configuration
was not observed by the agent.

### Verified Windows capture

The supplied UTF-8 CSV contains **28,253 data records**, all from `pes2013.exe`,
PID `15116`. The user confirms menu, kickoff, a full match and exit from the
game's menu. Replay coverage was not separately confirmed. The recorded process
exit status is `0`. Clock times cross midnight once, spanning 1,197.4894039
seconds; no capture date, frame timings or per-stage markers are supplied.
This is a loading reference, not a performance benchmark.

Both root-directory inputs were fingerprinted and moved unchanged into ignored
`private/research/engine/windows-capture-01/`:

- `pes13.CSV`: 5,310,758 bytes, SHA-256
  `2ba4cf0868de83fdbb9af9bf9d8a81fc162044c04438b9ea687690345396ae0d`.
- `pes13.PML`: 33,142,746 bytes, SHA-256
  `d4285b6da29b46597b679af7645a6c717f1e800d66b81704560dd775f4d92f86`.

The CSV columns are time, process, PID, operation, path, result and detail.
There are 80 successful archive-open events across **19 archives**, and 5,068
successful archive `ReadFile` events out of 5,328 `ReadFile` events overall.
The 16 archive basenames from the Mac run also open here; additional successful
opens include `dt00_e.img`, `dt05_e.img` and `dt05_r.img`. Missing alternate
language files are also probed. Such `NAME NOT FOUND` results are observations,
not evidence that the game failed: the run reached a full match and exited normally.
Windows and Wine event counts cover different recording layers and sessions;
their totals must not be treated as a performance comparison.

There are 126 image-load events, including `d3d9.dll`, `d3dx9_30.dll`,
`dsound.dll`, `dinput8.dll` and `XInput9_1_0.dll`, alongside ATI driver modules.
Repeated loads are not necessarily distinct modules. The main image loads at
`0x00400000`, with size `0x0188d000`. That agrees with the inspected executable's
image metadata but does not replace a SHA-256 comparison of the PC executable.

#### A ball-archive read to investigate

Event numbers here are one-based CSV data records, excluding the header.
`dt0b.img` has five successful read events:

- Events 21,459 and 21,463 read 2,048 bytes at offset 0.
- Event 22,257 reads 2,048 bytes at offset 2,048.
- Event **26,041**, at `00:00:19.2899644`, reads **81,920 bytes at offset
  5,992,448**. Event 26,042 repeats that range with `Non-cached, Paging I/O`
  flags. The duplicate range and paging flag do not establish two distinct
  asset requests.

The file's recorded end is 11,028,480 bytes, matching our inspected ball archive.
In that reference archive, entry 17 begins at 5,992,448 and contains 81,872 bytes;
entry 18 begins 81,920 bytes later. A separate direct AFS header/table read
confirms those values. This is the entry used for the native ball viewer.
The offset and length therefore support an entry-17 loading candidate in the
original Windows game. The user's PowerShell hash photo confirms PC archive
equality; the displayed ball choice remains unverified. The trace does not show
returned file contents or model-decoding instructions.

The selected read occurs about 105.320 seconds after process start, but no stage
marker labels it as kickoff. `dt10.img` has only two observed reads, both 2,048
bytes at offset 0; this alone cannot establish script execution or non-use.

The CSV has no thread IDs or caller stacks. The saved PML's selected stacks are
now decoded and cross-checked below. The supplied PowerShell photo reports
`pes2013.exe` SHA-256
`5b1dd4fc86b6e92696762caa0f92e4e59f026858131d1be6a0418d1d2b275694`
and `dt0b.img` SHA-256
`e77dddd5fb9df4362bf71ec16530ed6af4af748f9a9eba1ee73b71e1ebd13ad1`.
Both match the inspected Mac inputs. This is user-supplied PC hash evidence,
not a direct inspection of the PC files. The photo is preserved privately.

Bounded snapshots and before/after metadata checks protect the inspection from
input changes. Dictionary-based CSV totals agree with a separate positional-row
count. Analysis code and detailed records stay private; no parser dependency or
new production tool was added.

### Captured file-I/O stack addresses

A capture-specific standard-library reader inspected the saved PML v9 without
installing a package. Layout references were the
[procmon-parser format notes](https://github.com/eronnen/procmon-parser/blob/main/docs/PML%20Format.md)
and [reader source](https://github.com/eronnen/procmon-parser/blob/main/procmon_parser/stream_logs_format.py),
with its [MIT licence](https://github.com/eronnen/procmon-parser/blob/main/LICENSE)
reviewed. These are reverse-engineered layouts, not an official Microsoft
specification. Input size, table counts, ranges and stack depths are bounded;
hashes and before/after file metadata agree with the preserved inputs.
The narrow analysis script and full outputs stay ignored beside the capture.

The PML contains 241,842 events across multiple processes, rather than just the
CSV's filtered PES records. Only PES is interpreted here. Its process-table
index is `555`, PID `15116`, recorded as a 32-bit process. There are 29,450 PES
events: excluding 1,197 profiling events leaves exactly the CSV's 28,253 records.
All relative timestamps agree to their 100 ns precision, with no discrepancies.
All 5,328 read offsets/lengths match the CSV. Another 7,609 filesystem paths match
exactly; 103 directory-query records match their directory prefix, with returned
child-name decoding outside this reader's scope.

The selected ball read, CSV event 26,041, is PML event **48,713**, record offset
`0x00d3a249`. It runs on thread `2788` and has 18 captured stack addresses.
The captured user addresses pass through WOW64, `ntdll.dll` and
`KernelBase.dll`; the PES frame is **`0x004f7a0e`**, or **RVA `0x000f7a0e`**
relative to the recorded `0x00400000` image base. The same PES frame appears in
5,063 of 5,068 archive-read events; five have no captured PES frame. A shared
file-reading boundary is the working interpretation, not a ball decoder label.

The earlier ball opens, CSV events 21,457 and 21,461, also run on thread `2788`.
They capture PES addresses **`0x004f779e`** and **`0x004dc625`**. The later open,
event 21,903, runs on thread `8912` and captures **`0x00504b7f`**. These are
separate observed paths; no loading-thread roles or function names are assigned.

Captured stack addresses may be return addresses. They are not proven function
entries, complete call graphs or recovered instructions. The Windows module
table maps them to PES, and the matching PC executable fingerprint permits
inspecting those locations in the Mac copy. No original code was executed,
modified, unpacked or ported. The current on-disk code limitation remains.

Ghidra's check of these addresses is recorded below. The saved trace is sufficient
to identify the locations; observing the running game is the next step toward
their actual file-loading behaviour.

### Ghidra check of the captured frames

REA is now available in the chat. Ghidra 12.1.4 opens the unchanged PE32/x86
input with the expected SHA-256 and completes its default auto-analysis.
`resolve_containing_procedure` returns `found: false`, reason
`not_in_procedure`, at all four captured addresses:

- `0x004f7a0e`: `.text`, file offset `0x000f6e0e`.
- `0x004f779e`: `.text`, file offset `0x000f6b9e`.
- `0x004dc625`: `.text`, file offset `0x000dba25`.
- `0x00504b7f`: `.text`, file offset `0x00103f7f`.

For each address, `read_bytes` returns the complete 64-byte window starting
32 bytes before the frame. All four windows exactly match an independent
DOS/PE section-table mapping and direct source-file read. Each Evidence record
has the expected executable digest and provider version. The executable's
SHA-256 is unchanged afterward. The temporary Ghidra session closes successfully.
Full Evidence and checked mappings remain ignored under
`private/research/engine/ghidra-file-io-01/`.

This confirms the imported bytes at these locations, but does not recover
function boundaries, callers, instruction flow or pseudocode. A negative
procedure query does not prove that a function cannot exist there at runtime;
the transformed-code interpretation remains a hypothesis. No addresses were
forced into functions and no decoder, unpacker or header patch was applied.

CrossOver's existing WineDbg starts in the `PES 2013` bottle and its
`--command 'info proc'` query lists Windows processes. The first runtime
observation is recorded below.
WineDbg's [upstream manual](https://github.com/wine-mirror/wine/blob/master/programs/winedbg/winedbg.man.in)
documents process enumeration, attachment, backtraces and detachment.

### Runtime file-read observation

Launch the game under `C:\windows\system32\winedbg.exe` for this CrossOver
setup. Starting the 32-bit debugger attached to PES caused a restart into a
64-bit debugger and a second attachment failure. Direct late attachment with
the 64-bit debugger then produced a WOW64 fault in the interrupt thread and
PES exited. A 64-bit command-process probe alone did not cover this failure.
Launching a temporary 32-bit Wine command under the debugger worked; an
import-address breakpoint captured its read of a 21-byte text fixture, the
successful return and matching bytes. Breakpoints were removed and it detached.
The [upstream implementation](https://github.com/wine-mirror/wine/blob/master/programs/winedbg/winedbg.c)
also shows the 32-bit debugger restart after initial attachment. Do not assume
late attachment works because enumeration or a 64-bit probe succeeds.

PES was then launched under the 64-bit debugger from its installation directory.
It reached its normal PE entry point, and one startup read was observed:

- Wine process `0x00e4`; PES module `0x00400000`–`0x01c8d000`.
- `ReadFile` import slot `0x013f8174` resolved to `0x7bebe7cc` in `kernel32`.
- The stack return address was `0x004e7b41`. The six bytes immediately before it
  encode a call through that import slot. This is an observed call site, not a
  recovered containing function or the earlier archive-read frame.
- Arguments: handle `0x00e4`, requested length 4,096 bytes, a writable buffer and
  byte-count pointer, and a null overlapped pointer. The handle and PID having
  the same numeric value does not identify the file.
- At the return address, `EAX = 1` and the byte count was 852. The returned
  16-byte prefix begins with `WECF` and matches the local 852-byte `settings.dat`.
  The independent Windows capture also records a successful 852-byte read of
  `settings.dat`. File identity is a correlation; the live handle name was not
  queried. No archive read or full-match capture is claimed here.

The observed six-byte call instruction differs from the independently mapped
on-disk bytes at file offset `0x000e6f3b`. This establishes a runtime/on-disk
difference at this site, without identifying its cause or recovering a routine.
The executable hash remains unchanged. Both remaining breakpoints were removed,
the debugger detached and exited successfully, and PES remained listed as
running. The user confirms the game responds normally afterward.

Commands, diagnostics, the fixture probe and checked findings stay ignored in
`private/research/engine/crossover-debugger-01/`. No process image was dumped,
no protection was bypassed, and no native replacement was implemented.
Next observe an archive read with its filename/handle and range, then establish
the original caller's boundaries and behaviour before specifying a port.

### How REA and Ghidra fit this evidence

The logs identify a concrete operation to investigate. Ghidra examines the
executable's machine instructions and produces an approximate C-like view.
REA exposes an MCP connection through which the agent can request Ghidra's
functions, strings, references, instructions and callers. In this arrangement,
REA is the bridge and Ghidra is the analysis provider; a second Ghidra MCP server
is not required. See [REA setup](https://rea.tools/get-started/) and
[its native-analysis workflow](https://rea.tools/guides/native/).

For the candidate ball read, the objective is to connect an original caller
address to its file/AFS loading logic, verify the inferred inputs and outputs,
then compare a small portable equivalent with original behaviour. A caller stack
can give an address to inspect; it does not by itself recover the function.
Installation and provider/client health checks now pass, and REA is available in
the chat. The saved MCP launcher decompiles a known synthetic PE function correctly.
The captured PES frame queries above now verify the static-code limitation at
those locations; no original file-loading function has been recovered.
The current on-disk code limitation still applies: logging and an MCP connection
do not make transformed bytes readable or automatically port the game.

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
