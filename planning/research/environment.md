# Local environment and mod installation evidence

Observed 2026-10-17. Planning work did not launch Victoria 2/EU4, build V2DLL, replace the game's DLL, or change the executable.

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
