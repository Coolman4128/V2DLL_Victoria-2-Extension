# Local environment and mod installation evidence

Observed 2026-10-17. Planning work did not launch Victoria 2/EU4, build V2DLL, replace the game's DLL, or change the executable.

## Workload correction: HPM

The user corrected the intended mod from GHM to **Historical Project Mod (HPM)** on2026-10-17 and explicitly authorized downloading it. Official upstream: https://github.com/arkhometha/Historical-Project-Mod ; latest published release selected: **0.4.6.3**, tag commit `6819d3214d6787247ce7558fa7376fc591cadc92`. Download/additive installation completed20:58:23 EDT; [S01HPM](../sessions/S01HPM.md) native baseline subsequently completed and was independently accepted at22:58:55.953 EDT. Existing GHM remains historical evidence and does not establish HPM compatibility or performance.

HPM installation evidence:

- Official release: https://github.com/arkhometha/Historical-Project-Mod/releases/tag/0.4.6.3 ; archive from the corresponding GitHub tag, not a moving development branch.
- Archive `HPM-0.4.6.3.zip`: 86,996,470 bytes; SHA-256 `38D6541F160C14CFBC8560A31E3BD0F35735A3E4ACD23DCD1BA6D0E701E51CB2`.
- Retained download, extracted content, upstream metadata and verification manifest: `%LOCALAPPDATA%/V2DLLForkResearch/HPM-0.4.6.3/`, outside Git.
- All 8,441 archive files matched exact sizes and Git blob identities from the pinned upstream tree. Archive paths were validated before writing; no executable/DLL/script installer was present.
- Installed only `mod/HPM/` and `mod/HPM.mod`; both were absent beforehand. GHM and game binaries were preserved. Installed 8,432 files / 267,006,715 bytes; every destination SHA-256 matched. Independent destination reread confirmed exact file set and all hashes, zero missing/extra/mismatched files.
- Descriptor displays `Historical Project Mod 0.4.6`, selects `path = "mod/HPM"`, `user_dir = "HPM"`, `replace_path = "history/provinces"`. The shortened descriptor name does not override the pinned 0.4.6.3 release identity.
- Native HPM original-Lua menu and fresh paused Belgium,January1,1836 setup: **PASS, independently reviewed actual captures**, with user manual setup after supported secondary-monitor pointer input failed. Window remained wholly on physical DISPLAY2; user's game/display untouched. Exact HPM launch argument/pinned install/separate generated HPM userdata establish selection; no custom menu logo inferred. Supported Alt+F4 exited0;17preexisting files and exact12save set unchanged, no restoration/overwrite required. New HPM userdata897files/357,934,882bytes retained separately outside Git. Full results/limitations and45anchor private index SHA-256 `D81540F6072BBB79BAA7797963B9F388E864CB1C4CE2DC5FD6E2DE5F2EFA2A5C`: [S01HPM results](../results/S01HPM.md), [handoff](../handoffs/S01HPM.md). Gameplay compatibility, fork-candidate behavior and performance remain **NOT RUN**; no1837timing claim. Historical GHM receiver observations do not satisfy HPM native-hook gates.

## Hardware and tools

- CPU: AMD Ryzen 7 9700X, 8 physical cores / 16 logical processors (Win32_Processor).
- Discrete GPU: NVIDIA GeForce RTX 5070 Ti. Integrated AMD Radeon graphics and a virtual display adapter also present. Actual renderer must be recorded in benchmark runs.
- OS-visible memory: approximately 93.3 GiB (Win32_ComputerSystem); do not label that exact installed DIMM capacity.
- Visual Studio Community 2022 `17.14.36414.22`; MSVC tools `14.44.35207` discovered. Project requests `v145`, so S01 must establish a working Win32 toolset build, likely by explicit v143 retargeting and validation. Do not assume `.slnx` support; building the `.vcxproj` directly is an available route.
- Python and 7-Zip are available. Profiler, Ghidra and debugger availability still need the narrow S01 discovery.

## Victoria 2

Install: `C:/Program Files (x86)/Steam/steamapps/common/Victoria 2`.

`v2game.exe` is 12,294,656 bytes, PE machine `0x14C` (x86), preferred image base `0x400000`, MD5 `63C5485FF00353F2BE8DAAA3F0DCF957`. The file-version resource did not report a version. Verify 3.04/expansion state in the controlled launch; do not infer exact patch version from path alone.

Repository reverse-engineering reference MD5: `1D1A97E42BD2DD65DEABD3E7F707B27D`. The different hash does **not** prove incompatibility. Read-only PE-section mapping found exact expected bytes at these four sites:

| RVA | Source purpose | Matched bytes |
| --- | --- | --- |
| `0x85E40` | POP daily entry | `55 8B EC 64 A1 00 00 00 00` |
| `0x1D77A0` | Embark owner check | `8B 81 C4 00 00 00 3B 87 C4 00 00 00` |
| `0x38F559` | Fleet cargo UI observation | `8D 88 A4 01 00 00` |
| `0x5DF550` | Main loop entry | `55 8B EC 6A FF` |

This is a sample, not a complete patch audit. S02 verifies every retained/required site and records any prepatched EXE feature. `lua51.dll` remains the pre-existing 11,264-byte version 5.1.4 library. Do not replace it during documentation work.

## GHM installation completed

The user explicitly requested installation and confirmed **Greater History Mod**, not GFM. Source: author's [GHM 0.6 release](https://www.moddb.com/mods/greater-history-mod-ghm/downloads/greater-history-mod-06-version), published March 21, 2026. The release is listed as a demo; pin it rather than following future uploads automatically.

- Archive: `GHM.rar`, 26,055,841 bytes.
- Published and locally verified MD5: `DB096E8C6A454B3B902F6E8ED2C6E390`.
- Local SHA-256: `31C3E3B6ED29BAA00DFF5BC703AACBE901D4E8509790553B77DD88C8348CBBAA`.
- Retained download/staging: `%LOCALAPPDATA%/V2DLLForkResearch/GHM-0.6/`.
- Installed `mod/GHM/` and `mod/GHM.mod` into the Steam game folder. Both were absent beforehand; no existing mod was overwritten.
- Extracted/copied 6,163 files, 136,956,780 bytes in aggregate. Every destination file's SHA-256 matched its extracted source; zero mismatches. Archive path entries were checked before extraction. No executable/DLL/script installer was included in the extracted file listing.
- Descriptor selects `name = "Greater History Mod"`, `path = "mod/GHM"`, `user_dir = "GHM"`, and `replace_path = "decisions"`.
- Top-level content includes common, decisions, events, gfx, history, interface, inventions, localisation, map, technologies.

Status: **installed and file-verified; launch/cache generation and gameplay compatibility not tested**. No older GHM release was layered in. S01's launch smoke test must verify that 0.6 is a complete usable package and whether additional dependencies are actually required; do not infer missing assets merely from the smaller archive size.

GHM data is intentionally different from vanilla. Benchmarks compare the same GHM package/save/config before and after fork changes. UI integration must account for GHM's interface changes. Keep downloaded game/mod content out of the fork's Git history.

## EU4 reference

Installed Steam EU4 launcher metadata reports `v1.37.5.0 Inca (491d)`. Its automatic-transport localization was inspected as described in [transport.md](transport.md). Runtime reference behavior remains to be captured in S19.
