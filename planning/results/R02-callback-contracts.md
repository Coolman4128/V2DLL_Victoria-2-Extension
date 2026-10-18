# R02 VIEWS callback source contracts

Observed 2026-10-17. Source lane only: no live process, debugger, deployment or game files changed. This report inventories the callbacks actually compiled from source, rather than assuming every slot is a transparent dispatch. Native caller/target decoding belongs to the separate R02 PE lane. **Source declarations and synthetic transaction tests do not certify native ABI, object lifetime or safe phase.**

Source checkpoint: `Source/V2/V2/V2TechButton.cpp` SHA-256 `4164E90F773E2A15C1045FBEE71E4B0EF224C41F7F8E8953E7BFBC1B8DFB9EBE` at inspection. Symbol names are authoritative if integration shifts line numbers. Required local EXE SHA-256: `62D48C204364DD706584777C2E2B3C7AB3C5F1DD0170872554943575D53D6648`. No native support status is inferred from that identity alone.

## Actual dispatch

| View | Vtable / index / slot RVA | Actual installed source callback | Declared incoming/return shape |
| --- | --- | --- | --- |
| Technology | `0xA17FA4` / 11 / `0xA17FD0` | `Update0` | ECX view; EDX ignored; no stack arguments; void; ordinary ret |
| Budget | `0xA059F0` / 10 / `0xA05A18` | `Tip1` | ECX view; EDX ignored; two stack words retBuf, element; pointer result; ret 8 |
| Production | `0xA0FECC` / 10 / `0xA0FEF4` | `Tip2` | Same source declaration as Budget |
| Politics | `0xA0E458` / 10 / `0xA0E480` | `Tip3` | Same source declaration as Budget |

`VIEW_THUNKS` defines eight functions, but the install table chooses only these four. `Update1/2/3` and `Tip0` are not installed by this family. `g_origSlot[n]` is a function pointer captured by legacy `PatchSlot`, not an expected original target in the current installer.

Update calls `OnViewUpdate` **before** the native original, then calls the original if non-null. Tooltip calls `OnViewUpdate` before the native original; stores its pointer result; runs `OnTooltip` afterward; returns the stored result. A missing tooltip original substitutes retBuf and still postprocesses it. Thus null original is not a safe production fallback and should be rejected before any slot write.

## Settings and native dependency closure

The current default `g_settings.buttons=true`; `ENABLE_BUTTONS=0` only removes one branch. It does not disable installation, production setup or tooltip reads. The following table includes every direct native memory/call boundary in the source callback closure. Native callees' own transitive calls must be traced separately before claiming a complete native profile.

| Path / condition | Native reads, writes or calls | Evidence still required |
| --- | --- | --- |
| Every non-null view | View pointer stored/compared in DLL globals; no native dereference until later branches | Object identity and callback phase; valid generation, thread and recursion constraints |
| Every view with buttons=true, except same cached configured pointer | `SetupButtons`: read pointer at view+`0x4C`; Technology calls container vslot byte `0x6C` with temporary GStr `selected_tech_window` | View layout; container type; target identity; one-stack-word thiscall cleanup and temporary-string consumption |
| Technology/Budget buttons=true | Host vslot byte `0x34` with temporary button-name GStr; absent child is skipped | Dynamic host/vtable target identity, return type, lifetime and no retained temporary |
| Technology/Budget child found | Copy 44 bytes from view+`0x60` / view+`0x5C` to static `g_glue[i]`; replace method at+8; call button+`0x54` object's vslot byte `0x04` with cloned observer | Full glue layout and all copied handler fields; observer registration/ownership/destructor contract; button/observable lifetime; target signatures |
| Production/Politics buttons=true | `SetupButtons` reads view+`0x4C`, but BUTTONS has no entries for either index, so returns false without child search or glue copy | These reads remain even though neither view has a legacy button |
| Production independent of buttons flag | `SetupHideColonialButton`: view+`0x4C`; container vslot `0x6C` with `factory_buttons`; host vslot `0x34` with `hide_colonial_states`; copy 44 bytes from view+`0x120`; patch method+8; call button+`0x54` vslot `0x04`; retain raw view globally | Same dynamic/layout/observer evidence plus production owner lifetime through later clicks |
| Every tooltip, result and element non-null | `OnTooltip` always calls element vslot byte `0x44` first; reads returned GStr capacity at+`0x14`, then inline data or pointer | Element concrete class and getter contract; string layout/readability/NUL lifetime. Budget and Production have this dependency too: view index is tested afterward |
| Politics tooltip with name `plurality` | `StripBareNumberLines` reads retBuf GStr as above, scans `strlen`, copies at most 1023 chars and filters lines; `GStrSet` rereads capacity, truncates to capacity, writes text+NUL and size at+`0x10` | Native return really is the writable string pointer; allocation/capacity layout, no aliasing or concurrent access. This changes completed tooltip text |
| Native original per installed slot | Calls `g_origSlot[n]` with ECX=view and EDX=0; tooltip forwards same two explicit stack pointers | Actual local native entry/caller/epilogue contracts; hidden register requirements; complete original-target dependency |
| Registered Technology/Budget observer click | Native glue dispatch reaches naked `Thunk0/1/2`; pushad/call `FireDecision`/popad/ret. `FireDecision` calls RVA `0x2DCE10` through `void (__stdcall*)(void*)` with static fake element | Glue dispatch calling/cleanup contract and all observer virtual methods; decision entry ABI, return, preconditions and phase. Source comment claims ret 4, but does not prove local bytes |
| Fake decision element | Install constructs 0x30-byte static element with pointer at+`0x14`, capacity127 at+`0x28`, text prefix `POLITICSVIEW_DECISION` plus decision name | Native entry must consume exactly supported fake fields and not retain/own/free the fake object; all transitive command effects and locality remain unproved |
| Registered production observer click | `HideColonialThunk` pushad/calls `OnHideColonialClicked`/popad/ret; helper flips DLL flag then calls RVA `0x2F3E70` with EDI=`g_hideColonialView` under pushad/popad | Native hidden-EDI convention, other live registers/stack, object generation, UI phase, refresh reentrancy and lifetime |

Both VCall helpers read object vtable and the indicated function pointer before calling it. `VCall0` assumes no stack arguments; `VCall1` assumes one pointer argument with callee cleanup. They force EDX=0 and return a pointer. Merely proving an executable's four top-level slots does not establish these dynamically chosen methods.

The cloned glue includes its native vtable, owner pointer and all remaining callback fields. Therefore dependency closure includes later observer methods, not only the replaced click method. Unlike the separate player-button setup, these two VIEWS setup routines do not clear unused copied handlers or compare a known glue-click signature.

`g_hideColonialStates` is consumed by `ProdListVisibilityThunk`, installed separately only when `factoryAllowConstructionInColonial` is true. That installer uses RVA `0x2F424B`, resumes `0x2F4255` / `0x2F42DE`, and reads state+`0x84`. This is a separate existing installer family, not new permission to alter colonial policy. If active, its interaction with the registered production button belongs in the full runtime closure. Button registration itself has no such setting gate.

## Register, stack, FPU and exception accounting

At source level the fastcall wrappers emulate ordinary x86 thiscall by consuming ECX and a dummy EDX. The no-argument Update declaration requires no stack cleanup; two explicit Tooltip arguments require eight-byte callee cleanup. For entry ESP=S, ordinary Update return would leave S+4 and Tooltip return S+12. The source forwarding calls are balanced under exactly those conventions. The PE lane must establish this against actual entries, callers and return paths.

These are normal compiler-generated functions. EBX/ESI/EDI/EBP preservation follows the compiler's ordinary x86 ABI, subject to examining the actual Release Win32 binary and native callees. EAX/ECX/EDX and flags are not preserved across setup/native/postprocessing calls as arbitrary live machine state. Native original is explicitly passed EDX=0; any native hidden EDX argument would be lost. Pre-original setup may change volatile registers, flags and floating state before native entry. Tooltip returns its saved native result through the compiler, but does not preserve arbitrary native volatile-register return side effects.

There is no FXSAVE/FXRSTOR, x87 environment/data save, XMM save or explicit control-word restoration in these wrappers, their naked click thunks or the production refresh wrapper. `pushad` protects general registers only; it does not save EFLAGS or floating state. The scoped text-manipulation helpers contain no explicit floating-point arithmetic, but native virtual methods, CRT functions and original bodies remain call boundaries. No source-only FPU equivalence is claimed. Frozen stabilization code/settings remain untouched.

No surrounding SEH/C++ exception boundary protects SetupButtons, SetupHideColonialButton or OnTooltip. Null checks do not prove pointed-to native objects readable or valid, and a native exception during registration can leave partial observer side effects. Static slot-install rollback cannot undo observer registrations that occur later.

## Lifetime and phase blockers

`g_configuredView`, `g_hideColonialConfiguredView`, `g_hideColonialView` and copied observers retain native identity/owner pointers across callbacks. Source search finds initialization and assignments, but no teardown/load generation reset or unsubscribe path for this family. Raw-pointer equality cannot detect destruction plus same-address recreation. It is not proof that callbacks are safe after load/new campaign/reload of GUI assets.

Partial registration also has semantics: SetupButtons returns true after any one matching button succeeds; later callbacks then skip retrying missing siblings. If none succeeds it retries; there is no native registration receipt or rollback. These are pre-existing behaviors documented here, not changed in this lane.

Source comments say slot selection was empirical and views can be recreated. They do not establish exact thread, UI initialization completeness, observer-list iteration rules, ownership transfer, concurrency exclusion or legality of calling refresh from a cloned callback. A view tooltip/update hook is not a verified simulation-command phase. No new command sender should reuse it.

## Narrow integration and meaningful transaction checks

An inert family builder may fix the four table/slot identities and accept individually reviewed original targets plus validation-only dependencies. Reject missing target, wrong view order/index, non-four-byte target encoding, duplicate/overlapping write locations, replacement equal to original, invalid image ranges, incomplete native evidence or a missing complete-closure gate. All expected pointer values must be derived from verified target RVAs and actual load base with relocation handling; never capture current bytes as expectations. A nonempty evidence string is bookkeeping, not proof.

Keep native MemoryAccess/backend and production invocation absent until the entire active native profile is supported. Publishing g_origSlot outputs must occur only after all four writes and protection restorations succeed. Preserve prior output values on every rejection or rollback, including null/non-null sentinels. This supplements the pure AddressProfile seam, which knows byte writes but does not publish legacy output pointers.

Recommended focused tests exercise the family transaction's safety contracts: mismatch at each of four slots or any validation-only dependency yields zero protection/write/output changes; wrong executable/load-base relocation yields zero changes; partial write failure on each slot includes that slot and all previous slots in rollback; protection-restore failure attempts restoration and reports incomplete rollback accurately; no successful output publication before complete success; all preexisting output sentinels survive failure. Count mutation callbacks to show rejection happens before mutation. These tests establish transaction behavior only. They cannot pass the ABI, layout, generation or runtime-phase gates above.

## Lane outcome

**Source closure inventoried; native ABI/lifetime/phase unresolved.** The most immediate blocked dependencies are dynamic UI method targets/layouts, cloned observer ownership and deferred callbacks, decision-entry fake-element contract, hidden-EDI production refresh, tooltip result representation and lifecycle/phase evidence. Do not mark R02's native exits complete merely because a pure four-slot transaction passes. No source fixes or setting changes are proposed by this report; the manager owns narrowly scoped inert transaction integration and continuation selection.

## Pinned vanilla/GHM asset narrowing

Read-only filesystem evidence from `C:/Program Files (x86)/Steam/steamapps/common/Victoria 2`; no game assets copied into Git. A recursive case-sensitive literal scan across **all interface files**, not just the four named views, found the following. A separate case-insensitive rg scan produced the same occurrences.

| Exact source lookup name | Vanilla interface | GHM interface override files |
| --- | --- | --- |
| `FE_ACADEMIES_BDSM` | Absent | Absent |
| `FE_RPROJECTS_BDSM` | Absent | Absent |
| `FE_BUDGET_DIPLO_BDSM` | Absent | Absent |
| `hide_colonial_states` | Absent | Absent |
| `selected_tech_window` | country_technology.gui:212 | country_technology.gui:212 |
| `factory_buttons` | country_production.gui:448 | No occurrence; no country_production.gui override |

Vanilla interface corpus: 120 files, including 59 `.gui`; GHM interface override corpus: seven files, including four `.gui`. The GHM files are country_diplomacy.gfx/gui, country_politics.gfx/gui, country_technology.gfx/gui and temp_frontend.gui. GHM has no country_budget.gui or country_production.gui. Its descriptor specifies `path = "mod/GHM"` and `replace_path = "decisions"`, with no interface replacement declaration. Thus inheritance of vanilla production/budget and GHM technology/politics overrides is the expected asset-resolution model; actual runtime resolution was not observed in this lane.

| Relative path under game installation | Bytes | SHA-256 |
| --- | --- | --- |
| interface/country_technology.gui | 12201 | `1FC6909B64CE2835E8CC8C6CB76C0983397884C94D816F4E4F391D87C4DA629F` |
| interface/country_production.gui | 50195 | `8E933E2384E79E2DC3A3162DB5C511FD1AF74DC761342820398313529CD18DC2` |
| interface/country_budget.gui | 38547 | `CA2543C70A0E5153922A07FA8290E9E421457780D49C251958A11A47E4EB0FF6` |
| interface/country_politics.gui | 49415 | `34C113D29B0990814404813F812D0A1A359DB6760CE852AD9AD0B7F90A43E3B0` |
| mod/GHM/interface/country_technology.gui | 12201 | `22FBC16F9C26F0ED75D3A5CF540DFCD61DD7046CBF7FB2E23BE080B0F5FE6D48` |
| mod/GHM/interface/country_politics.gui | 53300 | `326CFBC44C9E3B194897BB5EEA77A101C4738C5E8C1A12148798DE6C3F90B941` |
| mod/GHM.mod | 94 | `964751159215507664C8B334B5EC69A8D6E42620A8A201CD6D91AC881FE329AB` |

Corpus fingerprint method: recursively enumerate every regular file, sort relative POSIX paths case-sensitively, then SHA-256 of UTF-8 concatenation `relative-path + TAB + lowercase-file-sha256 + LF`. Vanilla fingerprint `BB56B84DFDBE52D4D45D5979B62204D18B3F9912E98A99330B15480DF0B90A36`; GHM override fingerprint `B999AA8177C47E341D4B53B0E1C2956082A9975A77B0A80BE3057BF99C8DCB67`. This records the exact scanned corpus for the absence finding; it is not a manifest of every file the runtime might load.

Conditional reachability conclusion: **if** runtime loads these pinned assets and the native lookup ABI/semantics are correct, neither Technology/Budget legacy registration nor production hide-colonial registration finds its requested child; each returns before glue copying, AddObserver, retained hide-colonial owner publication and deferred click dispatch. This narrows the pinned baseline's expected dynamic paths. It does not globally remove those contracts: other mod assets, dynamic controls, plugins or changed interface files could expose them, and wrong lookup ABI could fail before a null return. The generic family must remain unavailable unless the actual supported profile constrains these cases or proves their closure. No source branch was disabled based on this scan.

Technology still reads view+`0x4C` and calls FindWindow with `selected_tech_window`, then attempts the two missing child names. Budget reads its container and attempts its one missing child. Production still reads its container, calls FindWindow with `factory_buttons` and attempts missing `hide_colonial_states` independently of buttons=false; with buttons=true its otherwise empty SetupButtons also reads the container. Politics with buttons=true still reads the container even though its legacy button list is empty. All three tooltip wrappers retain the element GetName and GStr dependencies and Politics retains plurality postprocessing. Nothing about absent controls establishes FPU/register preservation, original-call ABI or callback phase.

## Immediate lookup/getter ABI prerequisites

`MakeStr` constructs a 24-byte local GStr: 16 data bytes, unsigned size at+`0x10`, unsigned capacity at+`0x14`. For names shorter than16 bytes it embeds NUL-terminated text and sets capacity15. `factory_buttons` is exactly15 bytes and uses this inline representation. Longer names use a pointer to the shared 128-byte `g_nameStorage`, capacity127 and actual length; `selected_tech_window`, `hide_colonial_states` and all three legacy button names use this branch. Current literal lengths fit the buffer, but MakeStr itself does not enforce storageSize before copying. This observation applies only to the current literal set.

Before any null result can make an absent-control lookup safe, the following must be established on the local executable:

1. View+`0x4C` is a live pointer to the appropriate native GUI container at this pre-original callback phase; its vtable is valid and has the expected concrete implementation at byte `0x6C`. Returned host similarly has the expected implementation at byte `0x34`.
2. FindWindow/FindChild take ECX=this plus exactly one pointer stack argument, clean four stack bytes, return a pointer in EAX, require no hidden register argument or hidden return-buffer argument, and treat EDX=0 as irrelevant. These are requirements of VCall1's actual signature, not proof from a method name.
3. The native method accepts this exact 24-byte MSVC-style string representation, uses the passed string only synchronously, does not free/retain its storage, and does not expect its allocator/constructor invariants to differ. Reentrant or concurrent use must not overwrite the shared long-name buffer during consumption.
4. Missing names return null under this host/method combination and do not throw, dereference a missing child, return a sentinel or produce an owning result requiring release. The callback checks only null and then immediately consumes non-null results as GUI pointers. Asset absence alone proves none of these behavior details.
5. Tooltip element vslot byte `0x44` must take ECX=element, no stack arguments, require no hidden return buffer, return a borrowed string pointer in EAX and allow EDX=0. GStrText then requires a readable string header/capacity and live readable NUL-terminated data for strcmp; it neither checks native string size nor proves ownership. The element and result must remain live through OnTooltip after the native original returned.

These immediate lookup/getter gates remain open even in a profile that proves all legacy registration targets absent. Synthetic successful/null lookup fixtures can test branching only; they cannot certify these native prerequisites.
