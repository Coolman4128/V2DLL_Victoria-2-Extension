# R02 focused transaction validation

The new family adapter is compiled and deliberately has no production profile or call site. The separately tested Win32 backend described below remains unbound to production. The adapter does not gate the independent legacy VIEWS loop. No candidate DLL was copied to the game. The runtime, native PE, callback/lifetime and independent root ABI reports provide separate evidence and limitations.

## Meaningful checks

| Check | Exact result | Evidence outside Git |
| --- | --- | --- |
| Clean Release Win32 build, v143 / SDK10.0.26100 | 6.19 seconds, 0 warnings/errors | R02/build-final/artifacts/build.log |
| Staged source identity | All 45 current source/project files match staged hashes | R02/build-final/source-verification.json |
| DLL/export/PDB verification | All3 PE32 x86; exact128 Lua export name/ordinal/forwarder pairs; no missing runtime imports; all3 PDB GUID/age match | R02/build-final/artifacts/static-verification.json |
| Focused pure family test | 2,068 assertions pass; MSVC Release Win32 /W4 /WX /O2 C++20 | R02/views-contract-msvc/test.log |
| Existing AddressProfile contract | 250 assertions pass | S02/boundary-contract-msvc/test.log |
| Actual OS scratch pages | 18 scenarios / 2,308 checks pass; same Win32 compiler checks | R02/views-win32-os-contract/evidence.json |

All relative private paths above are under `%LOCALAPPDATA%/V2DLLFork/`. Root independently reviewed code, actual build/test/static artifacts and the real OS result. The manager reviewed the OS backend's bounded allocation, VirtualQuery protection assertions, exact protection tokens, output withholding, failure cleanup and explicit persistent failure recovery. A repeat test run is unnecessary absent a new change or unresolved test concern.

The pure tests cover preferred and relocated bases, wrong mapped base, mapped extent/target overflow, unknown identities, per-slot/entry/read dependency mismatches with zero mutation, conflicting/duplicate dependencies, injected read/protect/partial-write/restore/flush failure at every transaction position, explicit rollback failure, and withholding all4 original outputs during every backend callback. The real OS fixture uses only its own 11 MiB VirtualAlloc allocation and does not execute fixture bytes or open another process. Its18 scenarios are success1, slot mismatch4, partial write4, protection failure8, persistent protection failure1. The persistent case reports RollbackFailed, restores slot bytes, withholds outputs and observes the remaining writable page before the test harness recovers its own allocation.

These tests certify the supplied-identity memory seam, not discovery of a native profile, ABI, thread exclusion, GUI lifetime, candidate startup or complete active closure. Free-form evidence strings are reviewed assertions, not self-authenticating runtime capabilities.

## Stable identities

- `ViewsBoundary.cpp`: `1C0CBC1B0843F731FD6D1901C894958D7FC21E77206DEF09546475308D317D37`.
- `ViewsBoundary.h`: `6C9DEC7C0D51D43760D8D5150DA44003A1D95C9E4E1A33E9A78296DC08168C08`.
- `views_boundary_win32_tests.cpp`: `2D812472FFD2013DDE8EB4E9B1ED9042C5FAE3935DB55E2E107E8F8E903A4F03`.
- OS evidence JSON: `DA64ECDCF01E9B853A682375FA8ABF0A5159DA0822E1749E9C6F202CA882E65F`.
- OS test EXE: `E04FD4C734AF8D806E1C38CD914390FD78D06F185D38EA8AAA05BBD5C75ED38F`.
- Final lua51.dll: `C5D01DE9081A305EBECD1F06700FC249482533923C62654EE7E2C50229EA3F2E`.
- Final lua51.pdb: `DD44C0891DC217172ED4FA54773A5C2613824EEA239B2258D358CBB20C9C9F1C`.
- Final eventmusic.dll: `271FD3D5739BE26BF9F36F4A552622A96386973ED4913E78CCEEE77DEC9E036A`.
- Final eventmusic.pdb: `EFD8A2DB8228D50084B272CFCE45D405981C1B93092979048CE504777F653591`.
- Final v2dll_mod.dll: `17E5929E87A0942278D0AF58A8EC18DA07F0103A1C1E80055B6788A10DD4C1C9`.
- Final v2dll_mod.pdb: `5D1415292CDE553A6065B8A08F3E39BC2308B3DAA6196CDA0379F29B1CA45D0C`.
- Shared `V2TechButton.cpp`: unchanged from accepted S02 checkpoint, `4164E90F773E2A15C1045FBEE71E4B0EF224C41F7F8E8953E7BFBC1B8DFB9EBE`.

Reproduce a clean product build with `tools/build-release-win32.ps1 -BuildRoot <new external directory>`. Focused private test projects named above compile the checked-in tests together with the focused modules; their logs retain the exact compiler configuration. No test or report changes the frozen FPU controls/settings or implies performance acceptance. No1837 timing run occurred.

## Final integrated background backend checkpoint

The earlier6.19-second build and18-scenario fixture are retained historical checkpoints. The latest current-source build is **R02/build-win32-backend-final**,6.31seconds,0warnings/errors; all**47** staged files match current source. Its static PE/export/forwarder/import and all3 PDB GUID/age checks pass. No game/debugger access occurred during this integration; the stopped live experiment remains pending independently.

New focused `Fork/Win32Memory.h/.cpp` provides MemoryAccess callbacks for a caller-supplied current-process mapped extent. It does **not** authenticate executable identity, establish native signatures, validate lifecycle/phase, authorize installation or wire legacy code. It captures PODbase/extent/systempage size by value. Reads validate every covered committed readable region and allocationbase before copying; mutations require a single page. Guard/noaccess/unsupported protections and range overflow reject before touching. SEH catches invalid source/output copy faults; failed writes can be partial and the existing transaction journal must recover them. Data uses RW without adding execute; already executable pages retain execute. Original protection tokens remain reusable for rollback retries, including after an underlying restore succeeded but its wrapper reported failure. Exclusive phase remains the caller's obligation, including query/protect races and whole-page effects.

The revised OS fixture delegates memory operations to this actual module. Failure injection only surrounds those calls; it no longer duplicates backend VirtualProtect/memcpy logic. **19 scenarios /2,404 checks PASS** under ReleaseWin32/W4/WX/O2. Added direct edges cover cross-page mutation rejection/validation reads, null/zero/overflow/interior allocationbase, invalid source/output SEH, guard bit retained, noaccess/uncommitted rejection, same-page nested tokens, repeated restore, data noexecute and RX->RWX->exactRX. The original all4 output-withholding/partialwrite/restore-recovery/persistentfailure cases still pass. The cache fixture initially assumed requested PAGE_NOCACHE became actual protection; VirtualQuery showed this host normalized it to ordinaryRO. Corrected assertions use actual protection and **do not certify cache-modifier activation**.

Protection behavior follows [VirtualQuery](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualquery), [VirtualProtect](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualprotect) and [Windows protection constants](https://learn.microsoft.com/en-us/windows/win32/memory/memory-protection-constants). In particular VirtualProtect's old token describes the first page, motivating explicit single-page mutation. No other-process memory APIs are present.

Private final backend evidence `R02/win32-backend-contract/evidence.json`: SHA-256 **FF631C15D4E31EDA8A0B6A60D32632CDD1E53D414BE0084A0377BD4F5AF2D37C**. New source hashes: Win32Memory.cpp **B5744087C14B6A647C9F7CC596A39B60DEE68001395724B6BE1B92ACB4648FA6**; header **3668515477397EDFBE0CFFA8EEC22A51BBE5CEE88D5AF08B97961E9BF8869AFC**; revised test **E9249637F4DE88A2D67DEC40AF1784AA8862167E731F0847C947A05067D29B3F**. Final DLL/PDB pairs:

| Artifact | SHA-256 |
| --- | --- |
| lua51.dll | 54088298AB06631C6C56BCD54493E9EE9A870F87DCA8DC6383D8079BF7FF5579 |
| lua51.pdb | B1F4E0F86C2A75ACEA9456ADD50801C724560EFFD9C9565F8589C68937E3E826 |
| eventmusic.dll | 89239EE637862BFA4BD23F5C68D8B16D3FE7E4C089B9D5DDE230AB67C0D3DBE9 |
| eventmusic.pdb | F67B50F910E923B5F53AFD70F8FF6342764F78C2996208638FE58FD996D2D59E |
| v2dll_mod.dll | 91E95B02B23BF612455A7C1CFBCC292A3837D6058DB769B5FDC9CDACD72F9116 |
| v2dll_mod.pdb | 399CEB92A43246575BFEEE191CFE6A4A0F36A28E7DFAAD1B5558F19F1BA74EB2 |

The final DLL's six previously independently reviewed code spans (Update0,Tip1/2/3,OnViewUpdate,SetupButtons) are byte-identical at the same RVAs to the earlier accepted build, independently compared in `win32-backend-contract/compiled-wrapper-comparison.json`. Source V2TechButton.cpp remains unchanged at its S02 hash. This preserves the prior compiled-wrapper evidence; it does not manufacture runtime acceptance for the final DLL. Factory references occur only in its declaration/definition and tests, with no production caller.
