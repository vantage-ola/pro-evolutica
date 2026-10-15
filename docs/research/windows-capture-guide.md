# PES 2013 Windows capture bundle

The three requested memory snapshots have been supplied and inspected. They
contain the live Windows call-site code needed for the current investigation;
see [the findings](pes2013-engine.md#verified-windows-memory-snapshots).
No further collection is currently requested. The targeted debugger procedure
below remains a reference if later analysis identifies a specific missing value;
it has not produced a verified Windows breakpoint capture.

Collect this bundle in one visit to the Windows PC. It covers the current
file-loading investigation and preserves live state for later analysis. It cannot
record every future engine question: following a caller may reveal a new value
or operation that needs another targeted observation.

We already have a Windows full-match PML/CSV and matching `pes2013.exe` and
`dt0b.img` hashes. Keep those existing files. They do not need to be collected
again unless the installation changes. CrossOver also supplied a verified
`dt06.img` archive open, seek and read, plus part of its successful read routine.
The remaining comparison needs code and argument values from native Windows.

## Prepare one folder and the capture tools

1. Use the existing Windows installation, without adding a mod or replacing its
   executable. Create a new empty output folder, for example `C:\PES-capture`.
   Substitute your folder in the commands below if necessary.
2. Transfer the official portable [x64dbg release](https://github.com/x64dbg/x64dbg/releases)
   to the offline PC and extract it. Run **x32dbg.exe**, because this PES process
   is 32-bit. Keep using the existing [Process Monitor](https://learn.microsoft.com/en-us/sysinternals/downloads/procmon).
3. For the broader memory snapshots below, also transfer and extract Microsoft's
   official [ProcDump](https://learn.microsoft.com/en-us/sysinternals/downloads/procdump).
   It can write ordinary diagnostic snapshots of a running process without
   installing itself as the system crash debugger.
4. Put the notes template at the end of this guide in `session-notes.txt` inside
   the output folder. Record the game installation path and tool versions.

The debugger pauses and memory snapshots are for examining code and state.
This session does not measure FPS or normal loading speed. Keep snapshots,
runtime code slices, stack bytes and logs in private research storage; they are
not public source or redistributable fixtures.

## Broader collection: live memory at three game states

For the widest useful bundle, collect these before attaching x32dbg. They retain
loaded code, data and process metadata so later questions are less dependent on
another visit to the PC. Each is a snapshot of one state, not a record of every
instruction executed. They do not establish how an algorithm works by themselves.

Allow several gigabytes of free space; full snapshots can be hundreds of
megabytes or more each. From PowerShell in the extracted ProcDump folder:

1. Start PES normally and leave it at the main menu. Run:

   ```powershell
   .\procdump64.exe -ma pes2013.exe "C:\PES-capture\menu.dmp"
   ```

2. Load an exhibition match, reach kickoff, and open the game's pause menu. Run:

   ```powershell
   .\procdump64.exe -ma pes2013.exe "C:\PES-capture\kickoff-paused.dmp"
   ```

3. Play a short passage, open an instant replay and pause replay playback. Run:

   ```powershell
   .\procdump64.exe -ma pes2013.exe "C:\PES-capture\replay-paused.dmp"
   ```

Write the teams, stadium and selected replay action in the notes. Wait for each
command to finish and report a successful write before proceeding. ProcDump's
default is a 32-bit dump for this 32-bit process even on 64-bit Windows; do not
add `-64`. If there is more than one PES process, use its Task Manager PID in
place of `pes2013.exe`. If access is denied, retry from PowerShell opened as
administrator. If the tool fails or the game closes, keep its error text and
continue with the other available evidence.

These commands copy diagnostic state. Do not patch the executable, modify
registers or disable protection to obtain a snapshot. Note the game state for
each dump in `session-notes.txt`; gameplay video is not needed.

## Targeted debugger session, if later needed

### Start and attach

1. Start Process Monitor and pause capture with **Ctrl+E**. Clear its current
   events with **Ctrl+X**. Add the filter **Process Name is pes2013.exe → Include**.
   Keep file-system and process/thread events enabled. Keep profiling events off.
2. Start PES normally and leave it at its main menu.
3. In x32dbg, choose **File → Attach → pes2013.exe**. Use a fresh debugger session;
   attaching while debugging another program terminates that earlier program.
4. In the **Log** view, right-click and choose **Redirect Log**. Set the output
   to `C:\PES-capture\debugger.txt`.
5. In the command bar, enter each of these lines separately:

   ```text
   log "PES base={p:pes2013.exe} thread={u:tid()}"
   bph pes2013.exe+F7A08,x
   bphwcond pes2013.exe+F7A08,dword:[esp+8]==14000
   ```

   These addresses are relative to the loaded PES module, so a different module
   base does not require manual address arithmetic. x32dbg numbers are hexadecimal:
   `14000` means 81,920 bytes. The condition selects the expected request size;
   **that size alone does not prove the read is from dt0b.img**. The new PML will
   identify the file, offset and thread for the actual observed call.
6. Start Process Monitor capture with **Ctrl+E**, then press **F9** in x32dbg to
   resume PES. Load an exhibition match. If necessary, return to the menu and
   load once more. If there is still no breakpoint hit, save the log and trace
   and note that result; do not keep repeating matches.

### When the breakpoint hits

1. Save `before-read.png`: the CPU view with instruction bytes, registers and
   stack visible. Save `call-stack.png` from the **Call Stack** view and
   `modules.png` showing the PES entry in the **Symbols/modules** view.
   A short or incomplete call stack is still useful alongside the raw stack.
2. Check the instruction at EIP. The CrossOver observation was a call through
   the `ReadFile` import. If Windows instead shows a different instruction,
   record that and skip the step-over/return commands below. Do not modify the
   game or try to make its code match CrossOver.
3. Enter these lines separately while stopped at the call. They record the
   registers/arguments, preserve the output-count pointer in a debugger variable,
   and save a small stack window:

   ```text
   log "PRE thread={u:tid()} EIP={p:eip} EAX={p:eax} EBX={p:ebx} ECX={p:ecx} EDX={p:edx} ESI={p:esi} EDI={p:edi} EBP={p:ebp} ESP={p:esp}"
   log "ARGS handle={p:dword:[esp]} buffer={p:dword:[esp+4]} requested={u:dword:[esp+8]} count_ptr={p:dword:[esp+C]} overlapped={p:dword:[esp+10]}"
   mov pes_capture_count_ptr,dword:[esp+C]
   savedata "C:\PES-capture\stack-before.bin",esp,80
   ```

   The argument layout is based on the previously observed x86 call. If a
   command errors or the displayed call differs, keep the screenshots and error
   log rather than guessing another layout.
4. Save these bounded live code windows. They cover the file-read/open addresses
   and nearby callers already identified in the PML, for a total of 16 KiB:

   ```text
   savedata "C:\PES-capture\file-io-code.bin",pes2013.exe+F7000,1000
   savedata "C:\PES-capture\open-caller-code.bin",pes2013.exe+DC000,1000
   savedata "C:\PES-capture\seek-callback-code.bin",pes2013.exe+104000,2000
   ```

   These windows do not establish complete function boundaries. They give us
   actual bytes to analyse, including surrounding instructions that screenshots
   might cut off. These small files support the immediate loader question; the
   optional broader snapshots above preserve other live process state.
5. If EIP is at the expected call, press **F8 once** to step over it. Save
   `after-read.png`, with EIP, EAX and the stack visible. Then enter:

   ```text
   log "POST thread={u:tid()} EIP={p:eip} EAX={p:eax} ESP={p:esp} bytes_read={u:dword:[pes_capture_count_ptr]}"
   savedata "C:\PES-capture\bytes-read.bin",pes_capture_count_ptr,4
   savedata "C:\PES-capture\stack-after.bin",esp,80
   ```

   Record the result even if the read fails or returns a different count. Do not
   change EAX, the count or any other game value. If another breakpoint/exception
   interrupts the step, record its screen instead of labelling it a read return.

### Save and finish

1. Enter `bphc pes2013.exe+F7A08` to remove our breakpoint. Choose **File → Export
   database** and save it in the output folder if available; this preserves the
   debugger's analysis metadata, but does not replace the saved code bytes.
2. Choose **File → Detach**, then close PES normally through its menu. Do this
   before exiting x32dbg, which would otherwise terminate its debugged process.
3. Stop Process Monitor capture. Save PES events in native format as
   `debugger-session.pml`, then also export `debugger-session.csv`. The PML is
   required because it retains information, including stacks, that CSV omits.
4. Disable x32dbg log redirection, or use the Log view's **Save** action if you
   did not redirect it. Confirm the log contains the PRE/POST lines or the
   recorded failure. Record whether the game responded normally after detachment.

## Configuration notes

The existing full-match trace already covers menu, kickoff, a full match and
normal exit. No extra reference run or gameplay video is needed for this bundle.
Note the game's resolution, graphics quality, controller and installed patches
or mods. Screenshots of the graphics and controller settings are useful if
easier than typing them. Note any error encountered during collection.

## Notes template and handoff

```text
Game installation path:
Same executable/archive as the previously hashed copy: yes / changed / unsure
Windows version/build:
x32dbg version:
Process Monitor version:
ProcDump version, if used:
Graphics settings and controller:
Installed patches/mods, if any:

Memory snapshots, if made:
  menu.dmp: successful write / missing / error
  kickoff-paused.dmp: successful write / missing / error
  replay-paused.dmp: successful write / missing / error
  Teams / stadium / replay action:

Debugger run:
  Loaded PES module base and process ID:
  Breakpoint hit: yes / no / error
  Action when it hit:
  PRE/POST thread IDs (from debugger.txt):
  Instruction at the breakpoint:
  Read result and returned byte count:
  Game responsive after detach:
  Errors or missing capture files:

Other problems noticed:
```

Transfer the whole capture folder back to the Mac in one batch, preferably as a
ZIP. Preserve the original PML and DMP files. Memory snapshots, runtime code bytes
and logs belong under the repository's ignored `private/research/engine/`
directory. Full dumps may include copyrighted assets, save data and local paths;
keep them local and out of the public repository. Their successful collection
will still need to be checked before reporting a verified result.

The x32dbg commands were checked against its official documentation; this guide
has not been executed on the offline Windows PC. References:
[attach/detach and database export](https://help.x64dbg.com/en/latest/gui/menus/File.html),
[hardware breakpoints](https://help.x64dbg.com/en/latest/commands/breakpoint-control/SetHardwareBreakpoint.html),
[conditions](https://help.x64dbg.com/en/latest/commands/conditional-breakpoint-control/SetHardwareBreakpointCondition.html),
[module-relative addresses and memory expressions](https://help.x64dbg.com/en/latest/introduction/Values.html),
[debugger variables](https://help.x64dbg.com/en/latest/commands/general-purpose/mov.html),
[saving memory ranges](https://help.x64dbg.com/en/latest/commands/memory-operations/savedata.html),
[log formatting](https://help.x64dbg.com/en/latest/introduction/Formatting.html),
[saving/redirecting logs](https://help.x64dbg.com/en/latest/gui/views/Log.html).
