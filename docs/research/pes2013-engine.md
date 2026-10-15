# PES 2013 executable reconnaissance

The next investigation follows the original executable. The asset viewer is a
format/rendering diagnostic; it is not a replacement football simulation.
Original file-I/O routines can now be inspected from the supplied Windows memory
snapshots. No gameplay engine function has been recovered or ported yet.

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
The archive observation below follows that boundary with a directly observed
filename, handle and seek position.

### Runtime archive open, seek and read

A second launch under `system32\winedbg.exe`, with `+file` diagnostics,
captures one startup read from `img/dt06.img`. The filename is read directly
from `CreateFileA`'s argument, and its returned handle is followed through the
seek and read. The loaded PES base remains `0x00400000`; Wine process `0x00e4`
and thread `0x0164` identify the corresponding file-log events.

- Open: return address `0x004f779e`, read access, existing file, sharing value
  `3`, flags `0x08000000`; result handle `0x264`. Wine's paired `CreateFileW`
  diagnostic records the same archive and returned handle.
- Seek: return address `0x005057a8`, handle `0x264`, distance low/high both
  zero and method `FILE_BEGIN`. The result low/high are also zero.
- Read: return address `0x004f7a0e`, handle `0x264`, requested 2,048 bytes,
  null overlapped pointer. At return, `EAX = 1` and the byte-count output is
  2,048. Wine independently records `NtReadFile = SUCCESS (2048)`.

This establishes the range `[0, 2048)`. Offset zero comes from the observed
seek and result, rather than assuming a null overlapped pointer means zero;
[ReadFile uses the current position for this synchronous call](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-readfile).
Close/reopen breakpoints monitor handle reuse; neither interrupts the observed
open-to-read chain. Seek/read return stops are constrained to their expected
post-call stack positions to avoid confusing another thread's return.

The returned 16-byte prefix begins with `AFS` and matches the archive's on-disk
prefix. Its entry count is 1,442; the existing AFS inspector validates the table.
The archive is 131,594,240 bytes, SHA-256
`4825704596316e728d2a9bd1cfa945b0abcedbdad5ed0ad06f378723f194860b`.
This is an initial header/table block, not a selected asset payload or decoder.

Six-byte runtime call sites at `0x004f7798`, `0x005057a2` and `0x004f7a08`
call through the expected `CreateFileA`, `SetFilePointer` and `ReadFile` import
slots respectively. All differ from their independently mapped on-disk bytes;
the executable fingerprint remains unchanged. The read return address agrees
with the common frame from the Windows archive trace. These observations connect
the captured addresses to actual API behaviour; containing function boundaries
and full instruction flow remain unresolved. The automatic backtrace omits the
direct seek/read caller, so the raw stack return addresses supply that evidence.

All breakpoints/watchpoints were absent before detachment. PES remained
responsive afterward, as the user confirmed. Diagnostics and checked findings
stay ignored in `private/research/engine/crossover-archive-01/` and
`private/research/engine/crossover-routine-01/`. Debugger pauses make this
unsuitable for performance measurement. No native replacement or process-image
dump was produced.

#### Observed read routine return

A third launch under the 64-bit Wine debugger stopped at the same `ReadFile`
return address, `0x004f7a0e`, for handle `0x264`. This is the already-correlated
`dt06.img` handle. The call requested `0x800` bytes; the API returned success
and the output count was also `0x800`.

The live instructions after the call test the Boolean result. On this successful
path they skip the failure branch, compare the requested count with the
returned count, advance the buffer and cursor by `0x800`, update state in the
record addressed by `ESI`, and leave the loop because its observed count was
one. The routine returns `1` to `0x00505e3f`, the return address of an indirect
callback in the surrounding caller. This supports a bounded description of
the successful read path and its interface; it does not recover the routine's
entry point or establish the meaning of every record field.

The failure branch calls through import slot `0x013f82a4`, compares the result
with `0x26`, and branches on mismatch. That branch was not executed, so the
failure result and error handling remain unverified. The reader's complete
routine boundary also remains unresolved. Captured live instructions and
register/output values are in the ignored private finding; only this concise
summary is public.

#### On-disk bytes at the observed read call site

Ghidra maps `0x004f7a08`, the six-byte instruction before the Windows PML return
address, to file offset `0x000f6e08` in `.text`. The matching executable stores
`4d 1f 10 ad 43 a0` there, which Ghidra marks undecodable; it also finds no
containing procedure at `0x004f7a0e`. During the CrossOver run, WineDbg instead
observed `ff 15 74 81 3f 01` at that address, a call through the `ReadFile`
import slot `0x013f8174`. These facts show that the on-disk bytes and the live
Wine instructions differ at this location. They do not establish why, and the
Windows PML contains no instruction bytes or registers to determine whether the
native Windows process has the same live instruction.

This comparison motivated a native Windows capture of the call-site bytes and
the `ReadFile` arguments and return value. The supplied snapshots below now
provide the bytes; historical call arguments remain unavailable. The saved
Procmon trace remains the source for which file range the Windows game reads.
Ghidra mapping evidence and this bounded comparison are preserved in
`private/research/engine/ghidra-callsite-01/`.

The Windows memory snapshots below now supply the live call-site bytes. A new
debugger capture is deferred while these snapshots are analysed. The existing
Procmon log remains the record of file path, offset and length.

### Verified Windows memory snapshots

The supplied bundle contains three full-memory minidumps, named `menu.dmp`,
`kickoff-paused.dmp` and `replay-paused.dmp`, totalling 1,403,745,629 bytes. These
state labels come from their filenames and the requested collection procedure;
the agent did not observe the PC screens. The originals remain unchanged under
ignored `private/research/engine/windows-capture-01/PES-CAPTURE/`.

A local standard-library script checks the MINIDUMP header, stream-directory
bounds, module records and captured memory ranges before mapping addresses to
file offsets. The full-memory payload uses
[MINIDUMP_MEMORY64_LIST](https://learn.microsoft.com/en-us/windows/win32/api/minidumpapiset/ns-minidumpapiset-minidump_memory64_list).
Unused zero stream entries are ignored. The WOW64 module list repeats the same
PES module record; identical records are deduplicated for identification. The
AMD64 system-information field describes the host, while the captured PES header
is PE32/x86, machine `0x014c`, at base `0x00400000`.

- `menu.dmp`: 352,427,301 bytes; SHA-256
  `b351826ebd8ab4aaf0f04b2c5aac9ee7db80276779ad6305b35cd8b78c4ab26c`.
- `kickoff-paused.dmp`: 516,061,439 bytes; SHA-256
  `a59752e510107e6ce2c1f04e4a850f016463de5fad437e058248b41fe75af520`.
- `replay-paused.dmp`: 535,256,889 bytes; SHA-256
  `8c679f0e27185e769fd1b457a36ea707596df1bbdcbc9fa05e30625b25cd9f7b`.

Independent `shasum` agrees with all three fingerprints. A second address-mapping
check using direct file seeks reproduces all nine extracted code windows: three
windows per dump, 16 KiB per state. Corresponding windows are byte-identical
across all three states. They contain the same `CreateFileA`, `SetFilePointer`
and `ReadFile` call instructions previously observed under CrossOver.

In particular, native Windows memory at `0x004f7a08` contains
`ff 15 74 81 3f 01`, the call through `0x013f8174`. The captured slot points to
`0x75bdedc0`, which an independent walk of the captured 32-bit `kernel32.dll`
export table identifies as `ReadFile`, in all three dumps. This closes the
missing native-Windows instruction comparison at the PML's common read frame.

The in-memory PE header also differs from the on-disk header: it records six
sections, entry RVA `0x1000` and `SizeOfImage = 0x0188c000`; the source has seven
sections, entry RVA `0x0188c030` and `SizeOfImage = 0x0188d000`. The captured
module-list size is still `0x0188d000`, and its timestamp matches the source.
These observations do not establish the transformation mechanism or a full-file
hash for the running image. The previously supplied PC executable hash remains
the installation fingerprint. No runnable executable was reconstructed.

#### Bounded file-I/O analysis from the captured bytes

Ghidra 12.1.4 imports the small memory windows as raw x86/Windows code at their
captured addresses. Disassembly from aligned entries following padding gives
these function bodies, with approximate C-like output saved privately:

- `0x004f7740`–`0x004f782a`: opens a path with `CreateFileA`, stores its handle,
  queries file size through the helper below and updates a state record.
- `0x004f7970`–`0x004f7a9d`: the read worker containing the observed call at
  `0x004f7a08`. It limits each positive-size chunk to `0x20000` (128 KiB), rounds
  it upward to a `0x800` (2,048-byte) boundary, seeks, calls `ReadFile`, examines
  the result and returned count, and updates buffer/cursor/state fields.
- `0x00505730`–`0x00505778`: a `GetFileSize`/`GetLastError` wrapper that writes
  low/high size words through an output pointer.
- `0x00505780`–`0x005057d4`: a `SetFilePointer`/`GetLastError` wrapper with an
  optional low/high position output.

Known Windows API argument counts and `__stdcall` conventions were supplied to
Ghidra's import-pointer types; otherwise its stack reconstruction was misleading.
The import names are checked against the unchanged source PE import table.
Function names, local variables and record-field meanings are still analyst
interpretations, not recovered symbols. The assembly is the primary evidence.

For the earlier entry-17 candidate, rounding 81,872 bytes upward to a 2,048-byte
boundary gives 81,920, consistent with the Procmon read length. This connection
does not establish the worker's input record for that historical call. A paused
memory snapshot does not reproduce its earlier register arguments, return value
or complete execution history, and the static error branches have not been
executed in a controlled check. The Windows x32dbg attempt is not claimed as a
successful breakpoint capture; the user reports PES recovered immediately after
detachment.

Full disassembly, C-like output, hashes, local scripts and the Ghidra project
remain under ignored `dump-analysis-01/` beside the supplied captures. The source
executable's independent SHA-256 is unchanged. No gameplay code was ported and
no additional Windows or phone collection is currently requested. The next
analysis follows its dispatch and a header-decoding helper below.

### Read-job dispatch and WESYS-compatible decoding

The captured code establishes how the file-read worker is submitted and called:

- `0x004f88c0` prepares a read request and submits the worker pointer
  `0x004f7970` through `0x00506030` at call site `0x004f89cd`.
- `0x00506030` locks the dispatcher and calls `0x00505ca0`. That function
  stores the callback at record offset `0x30`, its argument at `0x34`, and marks
  the request pending at offset `4`.
- The worker loop at `0x00505df0` waits on an event, calls the stored callback
  with its argument, then clears the pending flag. Its indirect call returns to
  `0x00505e3f`, consistent with the earlier CrossOver observation.

All three snapshots retain a dispatcher record at `0x016d81b0` containing that
callback and argument `0x016d8720`. Its pending flag is zero. This supports the
field interpretation; it does not identify an active job or associate this
retained record with the earlier entry-17 read. Some internal functions pass
arguments in registers, so their decompiler output is not a verified C calling
interface.

A separate helper at `0x01134d40` matches the known 16-byte WESYS header layout.
It checks that byte `2` is `1`, reads the compressed length at offset `8` and
decoded length at offset `12`, allocates an output buffer, and passes the payload
at offset `16` to `0x01302ac0`. It returns the buffer on success or null on a
decoder error. This function alone does not check the full WESYS signature or
establish input bounds checks.

The callee at `0x01302ac0` has the structure of zlib 1.2.3's
[`uncompress()`](https://raw.githubusercontent.com/madler/zlib/v1.2.3/uncompr.c):
it initializes a stream with version string `1.2.3`, invokes the inflate routine
with finish mode `4`, updates the output length on success and cleans up the
stream. This is a structural identification from captured instructions, not an
execution check of the original decoder.

Verification compares ten bounded memory windows, 148 KiB per snapshot, across
all three dumps; all corresponding bytes match. Five key call/pointer
instructions are checked against the disassembly. The saved ball entry has
81,845 compressed bytes and a declared output of 748,000 bytes, consistent with
the fields this helper reads. Our existing Python decoder reproduces the saved
decoded file exactly, and its 33 focused tests pass. The unchanged compressed
and decoded ball payloads were not found in the snapshots; a buffer may have
been released or transformed, so this does not establish what was displayed.

These first windows establish the reader and decoder separately. The next
section follows their callers and shared buffer fields. Private scripts, checks
and the Ghidra project for this first step are saved under
`windows-capture-01/loader-trace-01/`. No original instructions were executed
and no Android loader was added in this step.

### Resource IDs, archive entries and the decode stage

The higher-level loader now supplies a static route through archive selection,
entry-stream reading and decompression. This combines decoded calls, captured
function tables and buffer-field use; it is not a recording of the entire route
executing during a match.

**Archive selection.** `0x01135360` first checks an alternate-resource lookup at
`0x01136d20`. On the base-archive path, the upper byte of a 32-bit resource ID
selects a holder from `0x018d9c98`; the lower 24 bits select its entry. The holder's
field at `0x0c` points to an archive object. `0x01251f90` searches attached
providers before calling that object's index-lookup method. In the four checked
archives this method is `0x004ac170`, which creates an entry stream through
`0x00d3ade0` when the object's alternate dictionary is absent.

Selectors `0x0b`, `0x0c`, `0x0d` and `0x0e` match the earlier registry records for
balls, the two player archives and animations. Their captured archive paths,
counts and complete entry-size arrays agree with the local files: 37 entries in
`dt0b.img`, 6,874 in `dt0c.img`, 3,042 in `dt0d.img` and 236 in `dt0e.img`.
The metadata consumed by `0x004f5de0` stores the initial displacement and entry
lengths. Its accumulation of lengths rounded to 2,048-byte units reproduces
**every entry offset** in those four AFS indexes, in all three snapshots. The
function also contains a compact sector-length branch; the checked archives use
the byte-length branch.

**Reading.** `0x011354a0` obtains an entry stream, allocates or reuses a buffer,
submits the read and waits for completion. The stream's size method rounds the
actual entry length upward to 2,048 bytes. Its read method, `0x00d3a840`, opens
the selected archive entry and converts byte ranges to sector units. The request
passes through `0x01175410`, `0x004f5c70`, `0x004f5630` and `0x004f5560` into the
file scheduler. The scheduler's context handler `0x00500410` calls
`0x00503970`, which invokes device-table slot `0x20`. The WIN table at
`0x01403640` places `0x004f9380` in that slot, connecting to the previously
traced background worker and `ReadFile` call.

**Decoding.** The read stage stores its buffer and length at loader offsets
`0x48` and `0x4c`. The completion routine `0x011356c0` either transfers that
buffer directly or passes it to the header-aware object at `0x01134ed0` and
dispatcher `0x01135000`. Type byte `0` copies the stored payload; type `1` calls
`0x01134f30`, then `0x01134df0` and the zlib-compatible decoder. Ownership fields
control whether the input buffer is freed. These field meanings come from the
instructions; exact C interfaces and all error paths remain unverified.

The earlier caller candidate `0x00cc10fc` is also a real call to the smaller
header helper. Its containing function, `0x00cc0f90`, walks named 12-byte bundle
records and treats `.tex` entries separately. It supports whole-bundle and
per-entry decoding on different branches. This does not establish that the ball
uses this particular named-bundle consumer.

**Working base-archive check.** Applying the inferred ID layout to ball entry 17
gives candidate ID `0x0b000011`. A private check selects `dt0b.img`, reads offset
5,992,448 and its actual 81,872 bytes, then uses our existing WESYS decoder. Both
the input and all 748,000 decoded bytes match the previously saved sample.
Its padded length is 81,920 bytes, matching the earlier Procmon range. The ID
itself was derived, not observed as a historical call argument, and the
alternate-resource route can take precedence.

Verification compares 14 code windows, 184 KiB per snapshot, across all three
states and checks the emitted instruction bytes against their captured source.
Direct-call targets and both relevant function tables are checked separately.
Mutable device records are kept separate: two WIN records remain in the menu
snapshot, while neither later snapshot retains one in that device-record array.
Their absence does not show that the route was unused earlier. No original
instructions were executed. Evidence and the reproducible local check remain
under ignored `windows-capture-01/loader-chain-01/`.

### Base-archive loader prototype

`tools/pes_resource.py` implements the checked base route in Python: split the
32-bit ID, select one of `dt0b`–`dt0e`, read the exact AFS entry and decode its
WESYS wrapper. It reuses the existing parsers. The AFS reader now exposes a
bounded selected-entry API without scanning payloads or hashing the archive.
The WESYS decoder also handles the observed stored `00 01 00` layout, requiring
zero at offset 12. Alternate providers, loose files and the original scheduler
are outside this prototype's scope.

The public CLI reproduced two saved local samples byte-for-byte:

- Derived ID `0x0b000011`: `dt0b.img` entry 17, 81,872 input bytes and 748,000
  decoded bytes. Both hashes and all output bytes match the saved ball sample.
- Derived ID `0x0e000001`: `dt0e.img` entry 1, 3,928,288 input bytes and 3,928,272
  decoded bytes. The stored output matches the saved animation bundle and its
  recorded SHA-256 `7b2bb68d7efc91adf7ebcd5d90833bf36a73b0a80afd4b5b24d5c72d20f22d8c`.

Archive size and modification/change timestamps remained unchanged. Original
synthetic fixtures check both wrapper types, archive selection, exact entry
reads, malformed indices/payloads, allocation limits, concurrent mutation and
CLI output protection. The focused AFS/WESYS/resource tests pass: 89 tests.
The full Python suite passes: 434 tests. Commands:

```bash
.venv/bin/python -m pytest tests/test_afs_inspect.py tests/test_wesys_unpack.py tests/test_pes_resource.py -q
.venv/bin/python -m pytest -q
```

The local comparison script, CLI reports and outputs remain under ignored
`private/research/engine/portable-resource-01/`. Usage is in
[TOOLS.md](../TOOLS.md#load-a-base-archive-resource-by-id). These are format and
saved-output comparisons. No original routine was executed, neither derived ID
was observed as a historical call argument. Equivalence to the original engine
routine remains unverified.

The corresponding C++ implementation in `native/assets/resource.h` and `.cpp`
now reproduces both saved samples and the Python output byte-for-byte in normal
and sanitizer builds. All eight native CTest targets pass. It builds into the
ARM64 APK using the existing zlib dependency. The viewer's optional L-key
diagnostic now reproduces both decoded buffers byte-for-byte on the Note20;
entry metadata agrees, and missing/corrupt input checks pass. The buffers are
saved for comparison rather than passed to the renderer. Original-routine
equivalence remains unverified. See
[the native verification record](../verification/native-resource.md).

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
The captured PES frame queries above verify the on-disk code limitation at those
locations. The later memory snapshots make the live file-I/O and header-decoding
routines inspectable. Logging and an MCP connection alone do not make transformed
bytes readable or automatically port the game.

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
assuming readable code or matching addresses. The supplied Windows snapshots
now provide a readable file-I/O code view. Trace its registration and callers
toward an archive-record consumer. Select one small function only after its instructions,
inputs and outputs are evidenced. Compare a portable equivalent with original
behaviour before calling it a port. Preserve the working Winlator installation.
