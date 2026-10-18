# Current-source Win32 build

On the recorded VS 2022 Community installation, run from the repository:

```powershell
./tools/build-release-win32.ps1
```

The script creates a new directory under `%LOCALAPPDATA%/V2DLLFork/`, copies current source/project files only, records source SHA-256 identities, and rebuilds `V2.vcxproj` directly. `-BuildRoot` selects a new external directory; `-MSBuild` overrides the executable path. The tested environment is MSBuild 17.14.19, MSVC v143 (19.44.35215), Windows SDK 10.0.26100.0, Release Win32. No x64 runtime support is claimed.

The global toolset override replaces the projects' requested v145 for this build without editing project defaults. It also applies to the EventMusic and V2Mod child builds. Separate project intermediate directories avoid shared-object collisions. The build does not use or overwrite committed binaries, deploy to Steam, change settings, or enable `/fp:fast`.

Artifacts are `lua51.dll`/PDB, `eventmusic.dll`/PDB, and `v2dll_mod.dll`/PDB. Builds include timestamps and absolute PDB paths, so successful clean reproduction does not imply byte-identical binaries across staging paths. Pin the exact artifact hashes used in each test.

S01 verifies exports statically; native loading remains gated by S02's complete hook verification. Before any later deployment, back up exact original DLL/config bytes. The installed native Lua chain consists of the 11,264-byte `lua51.dll` wrapper and `lua5.1.dll`; a candidate deployment must preserve the wrapper as `lua51_real.dll` and retain `lua5.1.dll`. Restore exact backups to roll back. Do not deploy an unverified candidate or replace the game executable.

Native smoke commands, from the game working directory, use the original Lua files:

```powershell
# Vanilla
./v2game.exe
# GHM: the .mod extension is required; omitting it silently selected vanilla.
./v2game.exe -mod=mod/GHM.mod
```

User settings, saves, generated caches and private smoke evidence remain outside Git. See `planning/handoffs/S01.md` for identities and actual launch results.
