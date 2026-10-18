# R02b independent root review

State: CODE, BUILD AND CONTRACT TEST REVIEW ACCEPTED. Final packet ledger/handoff/index review remains pending at this snapshot. This does not accept parent S02 or a candidate launch.

Root inspected the pre-change source at main-file SHA-256 `4164E90F773E2A15C1045FBEE71E4B0EF224C41F7F8E8953E7BFBC1B8DFB9EBE`. The sole direct legacy VIEWS installation loop selects a thunk and calls `PatchSlot` for each entry, publishing through `g_origSlot[i]`. Other references to that array are its declaration and the callback thunks; source search found no second named installer for this family.

| Family entry | Vtable RVA | Slot | Slot RVA |
| --- | --- | --- | --- |
| Technology update | A17FA4 | 11 | A17FD0 |
| Budget tooltip | A059F0 | 10 | A05A18 |
| Production tooltip | A0FECC | 10 | A0FEF4 |
| Politics tooltip | A0E458 | 10 | A0E480 |

The independent Budget UI installer shares vtable A059F0 but uses slot 6, so it does not alias the reserved Budget tooltip slot. Other named `PatchSlot` sites use UnitButtons A16BD0, MapIcon 9F8678, Building 9FDAC0/9FDAF0, or Decision A29B54. Blocking the four selected slots must not inadvertently reject those unrelated slots.

The public mod API exposes the shared slot helper and `StealToTrampoline`, plus the image base. Guarding only one callback is not evidence that all provided write routes reject a reserved-slot overlap. Interval checks must cover unaligned and equivalent RVA/index aliases before pointer arithmetic, protection changes, reads, or original-output publication. Arbitrary native plugins can also write memory directly; this work does not sandbox them or certify unsupported plugin profiles.

Root has not changed product source. Frozen simulation behavior and settings must remain unchanged; their prior uncertainty is not authorization for a new stabilization or OOS workstream. Final source routes, actual-source contract tests, build/static evidence and scoped frozen comparisons remain to be reviewed after integration.

Root completed the integrated source and compiled-route review against current main-file SHA-256 `73FFE1F373B93C32AF0D687963D7087A1B8B9077D19824D61C27BF9C3B8CEE1B`. Relative to the accepted R02 source, changes are limited to the include, reserved-span checks in the two legacy helpers, the logging-only skip helper and replacement of the old VIEWS loop. `Settings`, `LoadSettings`, the configuration fingerprint and all 19 recorded frozen function bodies match original commit `9a8c0d4484d15e79c98128804d39c550dbdea1ca`. Source comparison does not certify their pre-existing runtime assumptions or authorize a stabilization investigation.

The interval rule is correct for nonempty PE32 spans: an overlap exists when either span's starting byte lies in the other span. Unsigned distances cover wrap at 2^32, and the slot calculation uses unsigned multiplication/addition before the legacy pointer expression. Zero-length writes do not overlap. The checks cover source, declared trampoline capacity and original-output destinations; the original-output alias route in PatchSlot is also guarded. This is intentionally conservative about trampoline capacity, and leaves unrelated legacy behavior unchanged.

Root independently matched all 47 product source/staged/manifest entries and their exact file set. The actual clean build log reports Release Win32 success in 7.86 seconds, zero warnings/errors. Actual static evidence confirms all three PE32/x86 DLLs, 128 unchanged Lua export names/ordinals with correct forwarders, complete runtime imports and all three PDB GUID/age matches. Current lua51.dll SHA-256 is `47F9F62357FBD1CD6493ECB02F9F33C643E3FD5CE5D56DCB1AF506D81AE0B687`.

DIA against that build identifies PatchSlot at RVA9190/242 bytes, StealToTrampoline at15880/898, ApiPatchSlot at2F0D0/28 and ApiStealToTrampoline at2F0F0/40. The compiled reserved table contains exactly A17FD0/A05A18/A0FEF4/A0E480. Both shared helpers perform their integer overlap loops before native-memory access/protection/flush/output publication; overlap branches reach their false-return epilogues. Public cdecl wrappers marshal into the optimized internal functions and convert AL to the API integer result. The installer contains the unavailable-message push at2769E and Log call at276A3; the former installation loop is absent from reviewed source. No supported production profile or native callback is invoked. A first navigation listing began between instructions and was discarded; the accepted installer neighborhood follows boundaries decoded from the DIA entry. Private compiled-routes.json contains the exact bounded helper bodies, not a universal proof of the full legacy installer or exceptional paths.

Root reviewed the final actual-source harness **final-v4**, including extraction, source-call placement, real ModLoader wrappers, valid image extent and whole-instruction signatures, active assertions, and successful unreserved mutation controls. Actual tests pass:

- PatchSlot/API: 32/32 blocked-slot cases and 28/28 output-span cases.
- StealToTrampoline/API: 84/84 byte-alias cases plus 12/12 span-length cases; the latter distinguish five-byte source, ten written trampoline bytes and 32-byte declared capacity from a mistaken four-byte check.
- Rejections preserve the entire owned image and outputs and make zero protection/flush calls. Real Budget slot 6 and immediately adjacent slot controls remain writable. The skip helper reports once and does not mutate the image.

Earlier harness attempts had fixture/counting gaps and are not accepted as final evidence. Root requested the additional span-length cases; the final actual test source is SHA-256 `828DE5D95DA8E0DEFFADF9F2D37749BC867F69A4C8459472368573E9CECB49CF`. Root also read the current legacy regression result: 13 invalid arguments and six relative/split spans reject; 45 known/inventoried spans still reach mocked protection. All 2,068 pure family transaction assertions pass. The unchanged real Win32 backend's earlier 19 scenarios/2,404 checks are retained evidence and were not repeated. Synthetic helper execution does not certify native ABI or lifecycle.

Private root evidence: `R02b/root-compiled/source-audit.json` SHA-256 `1CFA974C9BCD4D588543C21F7FFE29F90AFEF5A504EC8D0F0DB238018407E5EF`; `compiled-routes.json` SHA-256 `459554F6C9C67D95538BF24359E3F05EB2F9603DE391A3042B05700CE49283B8`, plus independent DIA reader/output and scripts. Product code review found no outstanding issue. Final packet ledger/handoff/index consistency still needs acceptance before completion. Parent S02, remaining active groups, controlled candidate launch, performance and requested army features remain open.
