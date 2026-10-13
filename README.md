# pro-evolutica
android ports // reverse engineering of my favourite PES games growing up.

**A milestone-driven engineering project to investigate, run, and prototype Android support for classic Konami football games.**

Tool usage: [docs/TOOLS.md](docs/TOOLS.md). Verified work and pending device
tests: [docs/STATUS.md](docs/STATUS.md).

## Mission

Get **PES 2013** running as well as possible on a **Samsung Galaxy Note20 (Snapdragon 865+, Adreno 650)**, reverse-engineer enough of its PC engine and asset formats to build an **original Android ARM64 prototype**, and create a reusable research toolkit for **Winning Eleven 8/9**.

**Development machine:** MacBook Pro M1 Pro, 16 GB RAM, macOS.  
**Test machine:** Snapdragon Galaxy Note20, Android, connected over USB/ADB.  
**Delivery approach:** Work through independently verifiable milestones. Progress is determined by completed tests, not dates.  
**Primary game:** PES 2013, Windows PC version.  
**Secondary games:** Winning Eleven 8 and Winning Eleven 9 — *after checking the platform and executable format of each copy*.

> This is a reverse-engineering/compatibility research project, **not** a promise of a complete native PES 2013 port on a fixed deadline. A working Winlator configuration and a separate native asset-viewer APK are sensible initial targets; recreating or translating the full proprietary engine is a much larger research problem.

## Definition of done

### Minimum viable result

- [ ] PES 2013 tested on the Note20 via Winlator, with a reproducible configuration and a 10-minute match recording.
- [ ] Working tools that identify a Windows game's executable architecture, imported DLLs, hashes, and assets, with machine-readable reports.
- [ ] An inventory of PES 2013 asset containers and at least one validated asset extraction/inspection workflow, if a supported sample is available.
- [ ] A written technical decision: optimise a compatibility route, build native tools, or investigate hybrid translation further.

### Stretch result

- [ ] An installable **ARM64 Android APK** built from our own code that renders a test 3D scene and supports touch/controller input.
- [ ] Load and show **one verified, legally obtained PES-compatible extracted asset** if format and tooling allow; otherwise use a self-created asset and document the blocker.
- [ ] Reproducible benchmark comparisons across Winlator configurations; optional game-specific configuration improvements.
- [ ] Initial format/compatibility reports for Winning Eleven 8 and 9 (no assumption that all three games share interchangeable engines or assets).

**Not in v0.1:** Master League reimplementation, full 11v11 AI, original physics recreation, full native PES executable, online multiplayer, a browser release, or distributing copyrighted game files.

## Architecture: two parallel tracks

**Track A — Run the real games:**

```text
User-supplied PC game files
        -> Winlator
        -> x86 / Windows API compatibility (Wine + translation)
        -> D3D9 compatibility / Android GPU driver
        -> gameplay on Note20
```

Track A gives us playable results and performance data without rewriting the game.

**Track B — Learn and build original native code:**

```text
User-supplied PC binaries/assets
        -> static analysis (Ghidra, Python)
        -> documented formats + parsers
        -> portable C++ asset/runtime modules
        -> Android NDK / SDL3 prototype
        -> ARM64 APK on Note20
```

Track B is **not** a full PES 2013 port. Keep the two tracks independent so a failed native experiment never blocks gameplay testing.

## Technology choices

| Area | Choice | Why |
| --- | --- | --- |
| Binary research | Ghidra + **REA (morluto/rea)** + Codex; `file`, Python `pefile` | Let Codex ask evidence-backed questions of locally analysed Windows PE binaries on macOS |
| Asset inspection | Python 3.11+, `struct`, pytest, existing PES/WE format tools | Fast experiments and repeatable tests |
| Native modules | C++17/20 + CMake | Portable code and Android NDK compatibility |
| Android prototype | Android Studio, NDK, SDL3 | Window, events, controllers, graphics context |
| Rendering | Start with OpenGL ES 3; consider Vulkan later | Faster prototype on older hardware |
| Device tests | ADB, Android logcat, in-game FPS HUD | Reproducible diagnostics |
| PC compatibility | Winlator, Turnip, DXVK; WineD3D fallback | Existing ecosystem for x86 Windows games |
| Documentation | Markdown + CSV/JSON reports | Easy to share with Codex and reproduce |

## Getting started (Mac)

1. Install Xcode command-line tools and Homebrew (if missing).
2. Install the basic tools (including a Node.js version supported by REA):

   ```bash
   xcode-select --install  # skip if already installed
   brew install python cmake ninja android-platform-tools fnm
   eval "$(fnm env)"
   fnm install 24
   fnm default 24
   fnm use 24
   node --version # REA needs Node 22.19+, 24.11+, or 26+
   python3 -m venv .venv
   source .venv/bin/activate
   python -m pip install --upgrade pip
   python -m pip install pefile pytest
   ```

3. Install **Ghidra 12.1.x** (or the currently REA-supported version) from official releases, plus an ARM64-compatible **64-bit JDK 21+**. Confirm Ghidra launches and can decompile a known test PE file. REA needs a working local Ghidra native decompiler.
4. Install **Android Studio**, and from SDK Manager install the Android SDK, **NDK**, and CMake.
5. Configure **REA for Codex + Ghidra** using the steps in [REA + Codex integration](#rea--codex-integration) below.
6. Enable Developer Options and USB Debugging on the Note20, plug it into the Mac, authorise the connection, and verify:

   ```bash
   adb devices
   ```

7. Keep an **unmodified, legally obtained** copy of each game outside the repo. Start with PES 2013; add WE8/9 later.
8. For the Windows executable, inspect and fingerprint it:

   ```bash
   file '/path/to/pes2013.exe'
   shasum -a 256 '/path/to/pes2013.exe'
   ```

9. **Do not assume 32-bit vs 64-bit**. Record what inspection shows; choose a Winlator configuration with suitable 32-bit Windows application support if necessary.

## REA + Codex integration

**Repository:** [morluto/rea](https://github.com/morluto/rea)  
**Official setup:** [rea.tools/get-started](https://rea.tools/get-started/)  
**Native-analysis guide:** [rea.tools/guides/native](https://rea.tools/guides/native/)

REA (Reverse Engineer Anything) is an **MCP integration for coding agents**, not an emulator, decompiler replacement, or automatic porting system. It can connect **Codex** to a local **Ghidra** installation so the agent can search strings, inspect functions, follow cross-references and call graphs, and request assembly/pseudocode with supporting evidence. The agent still has to reason about the results and implement/test any replacement code.

On the M1 Pro Mac, use the *local* Codex client so the REA MCP server can access local files and Ghidra. Remote/cloud agent environments may not see your Mac's binaries or Ghidra installation.

**Set up (once, on the Mac):**

```bash
# Confirm a supported Node.js version and npm are on your PATH.
node --version
npm --version

# Set these to YOUR actual directories before running the commands.
export GHIDRA_INSTALL_DIR="/absolute/path/to/ghidra_12.1.x_PUBLIC"
export JAVA_HOME="/absolute/path/to/your/arm64/jdk-21-or-newer"

# Check that the Ghidra provider is usable before registering the agent.
npx -y rea-agents@latest doctor --provider ghidra --json

# Review and approve any changes to your agent configuration.
npx rea-agents@latest setup

# Restart Codex, then verify that the Codex connection is healthy.
npx -y rea-agents@latest doctor --client codex --json
```

During interactive setup, select **Codex** and **Ghidra**. Follow the diagnostic output if REA reports a missing JDK, native decompiler, or incorrect Ghidra path. **Do not assume REA is operational until both checks pass.** REA's setup may modify user-level Codex configuration; inspect its proposed changes first.

**First evidence-based analysis task for REA:**

1. Identify the local PES 2013 PE executable's hash and architecture with `tools/pe_inspect.py`.
2. Ask Codex + REA to open that local binary and identify its **actual imported DLLs/functions**, entry-point flow and likely file-loading code paths. Query specific functions one at a time.
3. For each claim, capture: executable SHA-256, address or symbol, assembly/pseudocode excerpt, cross-reference/caller evidence, interpretation, and confidence level in `docs/research/pes2013.md`.
4. Confirm a few findings manually in Ghidra. Don't infer subsystem names solely from decompiler guesses.
5. Use those findings to choose one **small, testable** function or data structure for a portable implementation; compare its output against known inputs when legal/practical.

**Privacy:** REA says binary analysis runs locally, but excerpts/tool results passed to Codex are handled by the model provider according to its policies. Keep game files outside version control; only share the minimum analysis output you intend to send to the agent.

**If REA setup fails:** keep moving with Ghidra's GUI and the Python inspection tools. REA is an acceleration tool, not a blocker.

## Execution plan: milestones, not calendar deadlines

### Milestone 1 — Repo foundation + first boot

- [ ] Create directory structure below, `.gitignore`, `pyproject.toml`, testing setup, and a short `docs/STATUS.md`.
- [ ] Verify optional **REA ↔ Ghidra ↔ Codex** integration with `doctor` on the local Mac; document actual version and status. Do not block coding if it fails.
- [ ] Build `tools/pe_inspect.py`: given a PE path, print JSON with executable hash, architecture, imported DLL/function names, entry point, sections, and file size.
- [ ] Add tests using tiny **synthetic/publicly redistributable** PE fixtures; no PES executable in Git.
- [ ] Set up official Winlator on the Note20: https://github.com/brunodev85/winlator/releases
- [ ] Try PES 2013 with **Turnip + DXVK** at **800 × 600, low graphics**. If rendering fails, test WineD3D and log the difference. Check that the container/runtime supports the executable's bitness.
- [ ] Write `docs/benchmarks/pes2013-baseline.md` with device details, Winlator version, driver/wrapper versions, settings, screenshots/logs, and whether a full match works.

**Exit check:** analysis tool works; first gameplay result is recorded even if it's a crash (including the error and settings).

### Milestone 2 — Establish reliable gameplay performance

- [ ] Troubleshoot crashes, startup movies, audio, cutscenes, and controller input one issue at a time.
- [ ] Benchmark at 800 × 600 and 1280 × 720, low settings first; then test higher quality if stable.
- [ ] Measure a **10-minute match**, including menus, match intro, set pieces, replay, and gameplay. Record average FPS and frame-time spikes/1% lows *where measurement tools allow*.
- [ ] Note thermal changes and performance after sustained play. Avoid interpreting a cool two-minute demo as sustained 60 FPS.
- [ ] Document the most stable configuration, not merely the highest peak FPS.

**Exit check:** repeatable gameplay baseline or a reproducible, well-described compatibility blocker.

### Milestone 3 — Investigate executable and asset formats

- [ ] Analyse a copy of `pes2013.exe` using **Codex + REA + Ghidra** where available (manual Ghidra fallback). Identify imports, graphics initialisation, file-loading routines, input interfaces, and candidate update-loop functions.
- [ ] Label only what evidence supports; maintain `docs/research/pes2013.md` with function addresses, executable hash, REA/Ghidra evidence, screenshots, and confidence levels.
- [ ] Inventory the game's real file extensions/containers. **Do not assume all PES releases use the same format**.
- [ ] Study existing PES 2010–2013 Kitserver research and standalone KTMDL tools.
- [ ] Build `tools/asset_inventory.py`: recursively list asset paths, sizes, extensions, and possible file signatures as JSON/CSV; do not require reverse engineering to catalogue files.

**Exit check:** documented binary map + searchable game asset inventory.

### Milestone 4 — One real asset, end-to-end

- [ ] Choose **one** supported asset/sample format based on what is actually in the game files.
- [ ] Reuse/adapt existing open-source parser code only after checking its licence and target file version; otherwise implement the smallest read-only parser.
- [ ] Parse at least one real sample successfully, with bounds checks, informative errors, tests, and checksum/metadata output.
- [ ] If the asset is a mesh, convert to a neutral format such as glTF/OBJ **for local research only**. If not, demonstrate a verified texture/metadata extraction instead.
- [ ] Make a decision memo: `docs/DECISION_001.md` choosing the next priorities based on measured results and actual format support.

**Exit check:** a tool that produces a verified output from a user-supplied asset; if blocked, record why and complete a synthetic-format proof of concept.

### Milestone 5 — Android C++ proof of life

- [ ] Create an Android application with **NDK + SDL3 + CMake** targeting `arm64-v8a`.
- [ ] Render a triangle, then a spinning textured cube using OpenGL ES 3.
- [ ] Implement input for touch and at least one supported gamepad.
- [ ] Build, install, and launch via `adb` on the Note20; save logs and a screenshot.

**Exit check:** native ARM64 APK runs independently of Winlator.

### Milestone 6 — Native asset viewer

- [ ] Keep parsers independent from Android UI and renderer.
- [ ] Add a small mesh/material loading interface with versioned test fixtures.
- [ ] Display the supported asset discovered in Milestone 4 (or an original replacement if parsing isn't ready).
- [ ] Add camera orbit/zoom, basic lighting, and error messages for bad files.
- [ ] Benchmark startup time and rendering stability on device.

**Exit check:** inspectable 3D scene in an Android APK. **Do not call it a PES game port.**

### Milestone 7 — Winning Eleven 8/9 reconnaissance

- [ ] Identify **which edition and platform** each game copy belongs to (Windows PC, PS2, etc.). A console disc image is not a Windows PE executable.
- [ ] For PC-era relatives, compare **PES 4 / Winning Eleven 8 International** and **PES 5 / Winning Eleven 9** tooling, but verify differences.
- [ ] Research Kitserver 4/5 and AFS tools; inspect one file without making assumptions about compatibility with PES 2013.
- [ ] Produce separate `docs/research/we8.md` and `docs/research/we9.md`.
- [ ] If a PC executable exists, attempt a Winlator boot and document results; if PS2-only, treat that as a separate console emulation/reimplementation problem.

**Exit check:** independent compatibility/format status report for both games.

### Milestone 8 — Polish, benchmark, publish source

- [ ] Improve reproducibility: dependency versions, build instructions, test commands, sample output.
- [ ] Run `pytest`, compile macOS tools, and build/install APK on the Note20.
- [ ] Record a comparative PES 2013 Winlator configuration report, including a result that is *not* improved if that is what tests show.
- [ ] Record demonstrations of Winlator gameplay and our native viewer as **separate accomplishments**.
- [ ] Publish original code, documentation, non-infringing fixtures, and technical findings. No game binaries/assets/keys.
- [ ] Create a backlog for potential future game-specific compatibility work or deeper engine reconstruction.

**Exit check:** repeatable setup and public technical demo without misrepresenting the scope.

## Proposed repository layout

```text
pes-android-lab/
├── README.md
├── .gitignore
├── pyproject.toml
├── docs/
│   ├── STATUS.md
│   ├── DECISION_001.md
│   ├── benchmarks/
│   │   └── pes2013-baseline.md
│   └── research/
│       ├── pes2013.md
│       ├── we8.md
│       └── we9.md
├── tools/
│   ├── pe_inspect.py
│   └── asset_inventory.py
├── tests/
│   ├── test_pe_inspect.py
│   └── test_asset_inventory.py
├── native/
│   ├── core/
│   ├── assets/
│   └── rendering/
├── android/
│   └── app/
├── examples/
│   └── generated_fixtures/
└── scripts/
```

Add directories incrementally. Do not create a huge empty codebase just to match the tree.

## Engineering rules for Codex

1. **Work in small, testable commits.** Show the commands used and their results.
2. **First inspect the repo**; preserve existing choices rather than replacing working code.
3. **No fabricated results.** If a game, file, APK, or hardware device is unavailable, say so and implement the testable next step instead.
4. **Separate facts from hypotheses.** Never guess engine function names or claim asset format compatibility without a sample/test.
5. **Do not attempt to recreate the entire engine upfront.** Build reusable readers, diagnostics, and rendering components.
6. **Prefer existing tooling** when reliable, but verify its licence, maintenance state, and supported game versions before integrating.
7. **Do not commit or package commercial game binaries/assets, decrypted content, serials, firmware, or bypass tools.** Do not bypass copy protection; rely on lawful user-supplied files.
8. **No hardcoded local file paths.** Provide CLI arguments, good errors, and `.gitignore` exclusions for `games/`, `private/`, extracted assets, build outputs, and logs containing sensitive paths.
9. **Track actual performance.** A 60 FPS target is a goal, not a result. Record crashes and regressions.
10. **Keep portable C++ logic separate from the Android shell.** All Android-specific APIs belong in platform adapters.
11. **Security:** bound-check binary reads, cap allocation sizes, handle malformed/untrusted files, and avoid executing unknown downloaded binaries on macOS.
12. Maintain `docs/STATUS.md` after each completed milestone, with blockers and next three tasks.

## First Codex session — start here

> Read this README as the project specification. Work on **Milestone 1, software-only tasks first**. Inspect the repository and report what already exists. Then:
>
> 1. Set up the smallest working Python project (`pyproject.toml`, `.gitignore`, pytest, directories as needed).
> 2. Implement `tools/pe_inspect.py` with a CLI: `python tools/pe_inspect.py --input /path/to/game.exe --json report.json`. Report SHA-256, PE architecture, entry point, file size, imports, and section names. Gracefully reject missing/non-PE files. **Never bundle proprietary inputs.**
> 3. Implement `tools/asset_inventory.py` with `--root`, `--json`, and `--csv` options; walk directories safely and report sizes, extensions, and magic bytes.
> 4. Add isolated tests that run without original PES files; create a few small synthetic fixtures if needed.
> 5. Create `docs/STATUS.md` and `docs/benchmarks/pes2013-baseline.md` with an empty, clear benchmark template for my Galaxy Note20. Record whether REA + Ghidra is installed and passes `doctor`, but do not install packages or modify my global Codex configuration without my approval.
> 6. Run all tests and show the exact results. Summarise changed files and tell me the **next manual step** to do on the phone.
>
> Once I provide the path to my local, lawfully obtained `pes2013.exe`, and REA passes its health checks, use REA to investigate the imports and file-loading entry points with traceable evidence. Do not invent findings if the file is unavailable.
>
> Do **not** start implementing a football engine, a full emulator, or an Android UI until those foundations are working. Do not claim to have tested Winlator or the Galaxy Note20 unless test evidence is supplied.

## Winlator benchmark record template

Copy this into a benchmark entry for each tested configuration:

```text
Game / edition / executable SHA-256:
Phone / chipset / Android version:
Winlator version:
Runtime / 32-bit compatibility mode:
Turnip version:
DirectX wrapper and version:
Screen resolution / game graphics settings:
Controller configuration:
Launch: pass/fail + errors:
Gameplay: menu / kickoff / full match / cutscene / replay:
Average FPS (measurement method):
1% low FPS or frame-time observations:
Thermal behaviour after 10–15 min:
Audio / input / graphical problems:
Evidence (local video or log path, not proprietary files):
Conclusion / next test:
```

## Existing projects and technical references

- [REA — agent-assisted reverse engineering](https://github.com/morluto/rea) and [official setup documentation](https://rea.tools/get-started/) — Codex + Ghidra MCP bridge for evidence-backed native binary analysis; not a game porting tool.

- [Original Winlator releases](https://github.com/brunodev85/winlator/releases)
- [Ghidra](https://github.com/NationalSecurityAgency/ghidra)
- [Android NDK + CMake](https://developer.android.com/ndk/guides/cmake)
- [SDL3 Android integration](https://github.com/libsdl-org/SDL/blob/main/docs/README-android.md)
- [PES 2008–2013 model/KTMDL tools](https://github.com/moth1995/pes2008-2013-tools) — research code; verify supported inputs and GPL terms
- [Kitserver tools for PES 2010–2013](https://github.com/pes-modding/kitserver-tools-2010-2013)
- [Kitserver 4: PES 4 / WE8 International PC](https://github.com/kitserver/kitserver4)
- [Kitserver 5: PES 5 / WE9 PC](https://github.com/kitserver/kitserver5)

## Project philosophy

**Ship experiments, not promises.** Get the original PC game running; collect data; use REA/Ghidra to reverse-engineer one evidenced subsystem at a time; write small native code that demonstrably works on the Note20. Reuse discoveries across PES 2013 and Winning Eleven only when the formats and engine versions actually match.
