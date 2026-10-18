# S02 executable and boundary verification

Status: **INCOMPLETE — complete active profile and controlled candidate launch remain blocked**. Native vanilla/GHM menus from S01 do not certify this DLL. No candidate has been deployed or loaded. No Steam executable, installed DLL/configuration, save, game process or UI was changed during S02.

Executable SHA-256 `62D48C204364DD706584777C2E2B3C7AB3C5F1DD0170872554943575D53D6648`, MD5 `63C5485FF00353F2BE8DAAA3F0DCF957`, PE32 x86, preferred base `0x400000`. Research-reference MD5 differs; no complete reference executable was available. See [binary inventory](S02-binary-inventory.md), [active dependency map](S02-active-dependencies.md), and [frozen boundary evidence](S02-frozen-boundaries.md).

| Check | Actual result |
| --- | --- |
| Independent source expectations versus local PE | 143 scoped literal comparisons / 129 distinct spans: 141 match; two external CombatRoll cave checks fail; all 21 byte-table originals match |
| Described external Roll Changer | Absent at all four identified roll sites; native inline divide-by-10 remains. No global unmodified-executable claim |
| Raw trampoline static spans | 41 resolved literal callers have local whole-instruction extents and no relative control instruction; this does not establish valid function entry or callback ABI |
| Frozen three entry/resume sites | Main-loop FPU, POP daily and daily/savings dispatcher signatures/stolen spans/resumes match; no stolen relocation starts or relative branches; caller and return details recorded separately |
| New boundary seam, pure logic | MSVC v143 Release Win32 C++20, `/O2 /GL /W4 /WX /fp:precise`: 250 assertions pass, zero warnings/errors. Unknown identity or any mismatched dependency produces zero writes; partial writes recover all attempted sites; recovery failure is explicit; validation-only dependencies never mutate |
| Actual legacy helper rejection harness | MSVC Win32: 13 invalid argument contracts and 6 relative/split instruction spans reject before mutation; four known spans plus all 41 inventory spans pass instruction validation through mocked protection; source/trampoline/original output unchanged |
| Current final DLL build | Clean external Release Win32 rebuild, v143 override and SDK10.0.26100.0, exit0, zero warnings/errors |
| Final exports/dependencies/symbols | x86 PE32; exact128 native name/ordinal exports; all correct Lua forwarders; original wrapper's128 imports resolved by installed Lua runtime; all three DLL/PDB GUID+age pairs match |
| Frozen source preservation | 19 frozen function bodies exactly equal HEAD, including FPU pinning, savings normalization, POP quantization and daily wrappers. Source equality is not runtime acceptance |
| Complete active profile | BLOCKED: unchecked expected vtable targets, runtime-derived partial signatures, compound call/data dependencies, dynamic imported-module/object-vtable boundaries and native ABI/lifetime/phase contracts remain |
| Controlled verified-profile candidate launch | NOT RUN; blocked by complete active profile. Disabling frozen remedies or launching only a convenient subset would not pass |
| Performance/gameplay/new commands/MP | NOT RUN; no claims. Existing MP stabilization remains DONE awaiting user testing |

The shared legacy trampoline helper now rejects missing expected bytes, missing required pointers, invalid image spans and capacities before accessing source. Its already-linked HDE32 decoder examines a zero-padded local copy of the verified span and rejects malformed, partial or relative instructions before protection changes. It does not relocate instructions or prove native ABI. Existing successful installation code still has prior protection/output-publication/rollback limitations and is not certified as transactional.

Legacy terminal startup logs now identify sequence completion rather than complete-profile success; exception handling reports the possibility of partial legacy state. An unconditional duplicate camera success message was removed. This improves reporting without changing frozen simulation logic. Actual native per-feature startup reporting remains unverified because no candidate launch is authorized by the current evidence.

`Fork/AddressProfile` is compiled into the project as a focused pure seam; no production profile, native backend or runtime feature is enabled through it. Exact executable identity, complete expected byte sets and native contract/evidence fields are required. Those fields are caller assertions requiring independent evidence. Successful synthetic tests do not establish native contracts. It supports read-only callable/resume/vtable dependencies and transactional writes, including rollback of a failing partially written site. Recovery/protection failures are explicitly distinguishable from successful rollback.

Private evidence root: `C:/Users/TylerW/AppData/Local/V2DLLFork/S02/`. Final build: `build-final/artifacts/`; exact staged-source identities: `build-final/source-manifest.json`; compiler log: `build-final/artifacts/build.log`; export/PDB evidence: `build-final/artifacts/static-verification.json`; pure seam: `boundary-contract-msvc/`; extracted helper: `legacy-contract-final/`; binary ledger: `binary-inventory.json` and summary; frozen proof: `frozen-boundaries/probe.json`; unchanged-source evidence: `frozen-source-preservation.json`. Earlier build/inventory evidence is retained separately and is not called final.

| Final artifact | SHA-256 |
| --- | --- |
| lua51.dll | `92DB3BF3D384465A18FC4D352E7CF28ED540FFD5849263F9A25374839AE22F0A` |
| lua51.pdb | `9F881EA98C2189E8DABA00F99B0369D7FDF27918C912129EC5B7AC232288D19D` |
| eventmusic.dll | `F4A7C780964682B094C25FECA6C20ED40E657E0ADB65C0D0CD9388A1193E3FF0` |
| eventmusic.pdb | `113D06EF264BD514A0A10FE0B0E43BA084178198608A8181D923EB124526F8B0` |
| v2dll_mod.dll | `31AC8287B445635D5D470D5B872F8F0A2CAD5867146016D57938523AD02B3AD3` |
| v2dll_mod.pdb | `4D892A4D8336320CFD7D1356783EA5B0B651F103B682286F14EF5406B75FA95A` |

Final inventory SHA-256: `F5128428335706A6CDCBD5A29C00161C10E4AD5C5590BC2E584D0E5E43CD3FA4`; final `V2TechButton.cpp` SHA-256: `4164E90F773E2A15C1045FBEE71E4B0EF224C41F7F8E8953E7BFBC1B8DFB9EBE`.
