# R02 owned Win32 CharLowerBuffA sample

`tests/fork/charlower_win32_tests.cpp` exercised the public Windows import in a private headless Win32 console process. It did not load the candidate DLL, open/inspect another process, invoke an engine function, create a window, control the desktop/debugger/game or modify game settings. Floating controls were changed only inside the owned test process and restored afterward.

The Release Win32/v143/MSVC 14.44.35207/SDK 10.0.26100.0 console build used `/W4 /WX /O2 /std:c++20`: exit 0, 0 warnings/errors, 0.91 seconds. The run exited 0 with **21 calls, 150 checks, 0 failures**. Seven pinned ASCII keys (the four custom-control names, `selected_tech_window`, `factory_buttons`, `start`) were tested under these three caller states:

| Owned process mode | x87 control word | MXCSR |
| --- | --- | --- |
| Test process entry |0x027F|0x1F80|
| Native-sample controls |0x007F|0x1F80|
| Masked round toward zero |0x0C7F|0x7F80|

Every call returned its exact span length, produced the expected ASCII lowercase bytes and preserved surrounding buffer guards. The x87 control word and MXCSR control bits (`0xFFC0`) matched immediately before/after the public API. MXCSR exception-status bits and arbitrary volatile machine values are not claimed equivalent. This is sampled input/control-state evidence, not a universal locale/error/ABI proof.

The test confirmed its imported function equals USER32's public export. Within this owned process, USER32's `api-ms-win-core-stringansi-l1-1-0.dll!CharLowerBuffA` IAT target matched the API-set-resolved export and its provider was KERNELBASE. GetModuleFileName from WOW64 reported `C:/WINDOWS/System32/{USER32,KERNELBASE}.dll`. A separate 64-bit file reader therefore hashed the **physical SysWOW64 x86 files**, confirmed PE signature 0x4550, machine 0x014C and optional-header magic 0x010B, and distinguished the native System32 PE32+ files in the identity artifact.

| Actual x86 file | File version / bytes | SHA-256 |
| --- | --- | --- |
| `C:/Windows/SysWOW64/USER32.dll` |10.0.26100.8117 / 2,023,488|`56F3BC77020E21EBEEF825965929DC0D0BD87F9CAFBB08DBCD477043E4342D2E`|
| `C:/Windows/SysWOW64/KERNELBASE.dll` |10.0.26100.9711 / 3,007,824|`5BE784046DC9731D1C36E872A8022065ED4A733707B1E467B30A96588714B137`|

Private `%LOCALAPPDATA%/V2DLLFork/R02/charlower-contract/evidence.json` records exact build/run commands, exit codes, source/project/artifact and system-module identities: SHA-256 `5BDF3A5F039186D063B2AD80B197C4DFC333DCD8E20DD17A8562F9DDCC813DEC`. Result JSON SHA-256 `EBD6856E12DCAC5FBE9211B714ADFB47531930DC76A7CAD5BAD0888E2034BDE9`; test source `D3DF463C04DF9227FBF2D7A8D9A95D3BF515A9C8FAA43BE1BE3B6A6992529B88`. Build/test stderr logs are empty. Full artifact anchors are in the private R02 index. Reproduce with the retained private `charlower-contract.vcxproj`, then run `out/charlower_win32_tests.exe`; it links the checked-in test source and public USER32 API only.

This narrows the observed lookup import's normal ASCII/control-state obligation on these exact system modules. It does not prove native lookup object validity, native helper reentrancy, arbitrary future Windows providers, exceptional behavior, the logging-enabled path, actual loaded HPM receivers or production startup. Runtime profile remains unavailable and candidate-loaded remains false. Product source/build and frozen FPU/settings were unchanged.
