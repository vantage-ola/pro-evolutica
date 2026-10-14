# Ghidra and REA installation verification

Ghidra 12.1.4 and REA 6.4.0 are installed locally under the ignored
`private/tooling/engine-analysis/` directory. They reuse the existing ARM64
JDK 21.0.11 and bundled Node 24.19.0. Codex has an REA MCP registration with
explicit runtime paths, a 30-second startup timeout and a 360-second tool timeout.
The previous configuration is backed up privately; comparison confirms all
unrelated settings are preserved.

The [official Ghidra release](https://github.com/NationalSecurityAgency/ghidra/releases/tag/Ghidra_12.1.4_build)
ZIP is 569,649,598 bytes. Its SHA-256 matches the
[published checksum](https://github.com/NationalSecurityAgency/ghidra/releases/expanded_assets/Ghidra_12.1.4_build):
`ddac49f903da9d5bac833e5cc79395098b9c33cfd3279be5f31bd00387d2d4db`.
Download connections stalled; preserved ranges were assembled and checked before
extraction. REA's pinned package version and npm integrity record are retained
privately with its lockfile.

The native decompiler was built using the existing compiler and stock bison,
following [REA's Apple Silicon instructions](https://github.com/morluto/rea/blob/main/docs/installation.md#macos-arm64-native-decompiler):

```sh
make -j4 ghidra_opt ARCH_TYPE='-arch arm64' OSDIR=mac_arm_64 ADDITIONAL_FLAGS='-w'
```

The resulting `decompile` executable is Mach-O ARM64. Verification passed:

- `doctor --provider ghidra --format json`: scoped `healthy: true`.
- `doctor --client codex --format json`: scoped `healthy: true`.
- The saved Codex launcher completes MCP initialization and advertises 170 tools.
- A generated original PE32/x86 fixture decompiles through both the CLI and the
  saved MCP launcher. At `0x00401000`, Ghidra returns
  `return param_1 * 3 + 7;`, matching the known instruction bytes.
- The fixture's SHA-256 remains
  `65a0eb0ecdfe07fba06a384c83e5713452b76f106ffda50a740a6a1ab26dd3de`.
  It was analysed without execution. The MCP analysis session closes successfully.

The initial restricted-shell analysis failed during headless startup; the host
retry passed. Doctor reports unconfigured optional engines, clients and the
REA skill as informational checks. Only the requested Ghidra/Codex setup was added.
Full commands, logs and evidence stay ignored in
`private/research/engine/tool-verification/`.

REA is now available in the chat. Direct CLI and MCP analysis are verified.
No PES engine function was analysed or ported during installation. The subsequent
[captured-frame check](../research/pes2013-engine.md#ghidra-check-of-the-captured-frames)
records the first game investigation through this connection.

The subsequent [runtime check](../research/pes2013-engine.md#runtime-file-read-observation)
verifies a 32-bit fixture read and one original PES startup read using CrossOver's
64-bit WineDbg to launch the target. Late attachment faulted in this setup.
The successful PES session removed its breakpoints and detached; the user
confirms the game responds normally afterward.
