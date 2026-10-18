# R02 original-game VIEWS observations

State: **IN PROGRESS - narrow original-Lua evidence only; no candidate DLL loaded**.

The integrator alone controlled the native game/debugger. The source and PE workers did not control that process. The user authorized computer use again; a later physical Escape briefly stopped it, then the user clarified that Escape was accidental and explicitly permitted continuation. The same pending process/debugger was revalidated and resumed, without treating a wait timeout as process completion.

## Identity and procedure

Original Steam EXE SHA-256 `62D48C204364DD706584777C2E2B3C7AB3C5F1DD0170872554943575D53D6648`; native `lua51.dll` SHA-256 `B2BA19343691F22D1AF1417F9A7CBAEAE74BA820A2A412F5A6CDEA271CC1A7B0`; native `lua5.1.dll` SHA-256 `E50C69219A676587199FCE1E27D9EF9F2924990F006B3F84CB37971F2E804215`. Menu showed Heart of Darkness 3.04, WNVU, without a mod argument. Working directory was the installed game directory. No fork configuration or DLL was deployed. No game executable on disk was modified.

GDB 16.3, configured x86_64-w64-mingw32, correctly selected i386 for this PE32 process. Private scripts, raw register/memory/disassembly observations and logs remain outside Git under `C:/Users/TylerW/AppData/Local/V2DLLFork/R02/`. Software debugger breakpoints are transient process instrumentation, not an installed candidate patch. These are diagnostic observations, not timing or gameplay-equivalence runs.

First experiment: owned PID `39368`, debugger exec session `2798`; two fresh Belgium 1836 setups in that process, never deliberately unpaused, no save command. The second setup followed resign/exit to main menu. The actual mapped EXE base was `0x210000`, independently obtained from its mapped `.text` start `0x211000` and PE section RVA `0x1000`. All four live VIEWS slot values equaled that base plus the independently decoded target RVAs in [the PE report](R02-native-views.md).

## Observed native entries and explicit returns

The three tooltip calls used native caller RVA `0x26A091`, returning at RVA `0x26A093`. The PE worker independently rooted that caller's instruction boundaries at entry `0x26A040`; arbitrary back-disassembly offsets in the raw log are not accepted boundary evidence. Each observed ECX object had the exact expected class vptr. The stack contained a return address, output pointer and element pointer in that order.

| Actual view | Entry VA / ECX | Entry ESP -> explicit return ESP | Return and preservation |
| --- | --- | --- | --- |
| Budget | `0x416780` / `0x4EB20D90` | `0x1AD6DC` -> `0x1AD6E8` | EAX=`0x1AD70C`, the supplied output; EBX/ESI/EDI/EBP unchanged |
| Production | `0x5110A0` / `0x4A270400` | `0x1AD6DC` -> `0x1AD6E8` | Same output-pointer return; EBX/ESI/EDI/EBP unchanged |
| Politics | `0x4E2DD0` / `0x4EB58F88` | `0x1AD6DC` -> `0x1AD6E8` | Same output-pointer return; EBX/ESI/EDI/EBP unchanged |
| Technology, exact slot 11 in second setup | `0x5BBD70` / `0x3B49CA50` | `0x1ABCD4` -> `0x1ABCD8` | Explicit return VA `0x5BCBC4`; EBX/ESI/EDI/EBP unchanged |

Tooltip ESP advances by 12 including the return address: the native function cleans eight argument bytes. Technology ESP advances by four including the return address and cleans no arguments. Technology's exact caller reads vptr `+0x2C` at RVA `0x3ACBBD`, sets ECX=view and calls at `0x3ACBC2`. Its observed new view vptr was `0xC27FA4`, exactly base plus `0xA17FA4`.

All accepted captures ran on initial process thread `0x9A68` (GDB thread 1). Native x87 control word `0x007F` and MXCSR `0x1FA1` were unchanged across the three sampled tooltip calls; the exact Technology slot-11 sample had unchanged `0x007F` / `0x1FB3`. These are native observed values, **not** the fork's frozen effective settings, a complete floating-register comparison, or proof of arbitrary native/fork callees' FPU behavior. Volatile registers and flags changed normally. No simulation-command phase is established by a UI callback sample.

The Politics output was a native GStr with heap storage, size `0x28`, capacity `0x2F`; dereferencing its data pointer yielded the plurality tooltip. The Budget object-address string dump in the raw log is excluded as text evidence because its capacity was `0xAF` and it uses heap storage. These observations establish those concrete representations only, not every possible result/element class.

The Politics element had vptr RVA `0xA38738`, and the sampled Production element had vptr RVA `0xA3A930`. Both observed slot `+0x44` targets resolved to RVA `0x2465D0`. The Politics descriptor at element `+0x30` had vptr RVA `0xA41D20`; its slot `+0x18` resolved to RVA `0x5ED1D0`, which returns descriptor+8. The observed inline descriptor string was `plurality_value`, size/capacity 15. The PE worker independently verified the static chain; no target was deliberately called by the debugger. This does not certify unknown descriptor types.

## Lifetime and exclusions

The first Technology view address `0x4EC5E098` contained the expected Technology vptr during the first setup, but contained zero at that address after resign and second setup. The second Technology view had a different address. This witnesses invalidation across recreation; it does not prove arbitrary address reuse safe, complete lifecycle exclusion, or new cached-object semantics. The new pure installer retains only static function addresses, not these view pointers. Existing source caches and cloned observers remain separately documented limitations in [the source report](R02-callback-contracts.md).

Excluded observations remain in the raw log for audit:

- Initial preferred-base breakpoint addresses were wrong for the ASLR-mapped process. The unrelated hit was inspected, identified as unrelated and all four breakpoints removed before installing correctly relocated ones. It is not VIEWS evidence.
- GDB `finish` on a frameless native entry unwound to an enclosing frame and produced an inappropriate stack delta. It is excluded. Accepted returns use explicit temporary breakpoints at the return address captured directly from entry ESP.
- The Technology target is shared by slot 2 as well as slot 11. A separately observed return at RVA `0x26C060` came from slot 2 and does not substitute for the exact slot-11 capture above.
- Arbitrary `return-16` disassembly may begin mid-instruction. Only independently rooted caller CFGs support instruction boundaries.

First experiment closed normally via Alt+F4; GDB recorded normal inferior exit, then quit. Generated settings were retained privately and the exact prelaunch settings restored. EXE, both Lua files, settings and all 12 existing saves matched: **16 matches / zero mismatches**. No save was created or deliberately loaded. Stable log `native-baseline/gdb.log` SHA-256 `47F07E963A867F1165B1392A720A37912D4D8FEE078BF355867234CA0121AED5`; identity checks are `native-baseline/identities-before.json` and `identities-after.json`.

## Second bounded experiment: native lookup closure

Owned PID `25124`, debugger exec session `84293`; original Lua and same mapped base. This experiment began with `starti` and mapped identity inspection before placing the Technology native creation lookup breakpoint at RVA `0x3AC068`. Physical Escape interrupted UI access while it was at the native menu; after renewed authorization, the same live process and debugger handle were revalidated. No replacement process was launched because of that interruption. Its raw log is `native-lookups/gdb.log`; final preservation evidence will be appended before handoff.

The naturally executing Technology creation routine passed the heap-form GStr `selected_tech_window` (size 20, capacity 31) to container `+0x6C` at call RVA `0x3AC068`, target `0x5E4A20`. The explicit return at `0x3AC06A` gave host `0x4EC36260` and popped its four-byte argument. Its next native call at `0x3AC076` passed inline GStr `start` (size 5, capacity 15) to host `+0x34`, target `0x5E4C80`; return at `0x3AC078` gave button `0x4EC39DB8` and popped four argument bytes. No debugger-injected call, key replacement, allocation, fake object or return-register restoration was used. The native caller then proceeded normally.

That button's observable at `+0x54` had vptr RVA `0xA3CF78`; slot `+4` target was RVA `0x247340`. The PE worker independently verified that the native registration helper retains the exact supplied observer pointer in a list node, rather than cloning it. Its removal helper releases/marks list nodes. This does not prove arbitrary static cloned observers safe through every native teardown.

Each immediately active lookup receiver was inspected separately:

| Source receiver | Actual pointer | Vptr RVA / FindChild target RVA | Embedded fallback vptr / root |
| --- | --- | --- | --- |
| Technology `selected_tech_window` | `0x4EC36260` | `0xA3C0D8` / `0x5E4C80` | `0xA41E84` / zero |
| Budget view+`0x4C` | `0x4EAF33B8` | `0xA3C0D8` / `0x5E4C80` | `0xA41E84` / zero |
| Production `factory_buttons` | `0x4A2C8310` | `0xA3C0D8` / `0x5E4C80` | `0xA41E84` / zero |

Budget view `0x4EAF2B38` had exact Budget vptr RVA `0xA059F0`. Production view `0x30010A28` had exact Production vptr RVA `0xA0FECC`, with container `0x30011058`. That container's native window-map begin/end at `+0x424` / `+0x428` described six 32-byte records. The inline `factory_buttons` key identified its child host at record `+0x1C`, corroborating the independently decoded map layout without calling a query helper. Both Production and Technology container slot `+0x6C` targets were `0x5E4A20`.

For all three actual receivers the fallback object begins at receiver+`0x370`, has vptr `base+0xA41E84`, and its tree root at receiver+`0x374` was zero. The independently verified fallback chain therefore returns zero if both preceding collections miss. These actual observations support that conditional branch for each receiver; a Technology constructor default alone was not generalized to Budget or Production. The pinned GUI corpus lacks the custom control names, but complete runtime asset-resolution and no-dynamic-creation constraints must still accompany any future supported profile. The custom observer branch is not generically certified.

The second Budget element used vptr RVA `0xA38738`, descriptor RVA `0xA41D20` and the same `0x2465D0` -> `0x5ED1D0` name-getter chain; its inline key was `tax_0_inc`. The second Production element used vptr RVA `0xA3A930`, descriptor RVA `0xA3CACC`, and again descriptor slot `+0x18` target `0x5ED1D0`; its inline key was `pop_icon`. Different descriptor types therefore cannot be inferred from one sample, although these specific native targets match.

Whole-family support, complete active-profile startup, observer lifetime, production invocation and candidate runtime acceptance remain **unproved**. Existing multiplayer stabilization source/settings were not changed, and no multiplayer improvement workstream was opened.

The read-only receiver snapshot `native-receivers/pid-25124.json` found no requested custom names in Technology lists (1+0 entries), Budget (7+0), or Production factory_buttons (10+0), each with a zero fallback root. Its historical field `observed_missing_keys_return_null` is an **inference from the observed lists/root plus the independently decoded native CFG**, not a missing-key call that was executed. The reusable script now labels this explicitly `missing_key_null_inferred` with its inference basis. Original snapshot SHA-256 `B4D51558D330C9E5F0F208E378B3A5A3274D9AF9D0EA987800FB7D9B2DA83408` is retained unchanged. PID25124 then exited normally, the exact prelaunch settings were restored, and EXE/both Lua/settings/all12 saves again matched (16/16). Its final log SHA-256 is `6A8AED2B69001CBCD093A35F971696B850FF9AD341C57BEB76DB8B99783D07DA`.


## Pinned GHM receiver confirmation

Original-Lua PID `22232`, GDB session `66709`, launched with `-mod=mod/GHM.mod` from the game directory. Its mapped `.text`/base was independently inspected before breakpoints. The branded native GHM menu displayed **TPSP** under GDB; S01 earlier recorded **CTFJ** without a debugger. All **6,163 installed GHM files matched** S01's manifest (zero mismatches), including the descriptor and relevant GUI files. No cause for the displayed difference is inferred, no checksum protocol is added, and no multiplayer investigation or fork-regression claim follows. No candidate DLL was loaded in either condition. Future benchmark fixtures need their own pinned identities.

A fresh Belgium 1836 setup remained at January 1. The native Technology lookup passed `selected_tech_window` and returned host `0x5B302768`; Budget view+0x4C resolved to `0x5B1970B8`; Production's native window map resolved `factory_buttons` to `0x3FA3DF98`. The read-only receiver script independently checked each actual subtype, target, name/object vector sizes and zero embedded root. As in vanilla, list counts were Technology 1+0, Budget 7+0, Production 10+0; all four requested custom keys were absent. `missing_key_null_inferred` is explicitly derived from these observations plus the reviewed native CFG, without invoking a missing-key lookup. Snapshot `native-receivers/pid-22232.json` SHA-256 `A991D3E46049B6A71B5EA52E8E0BAF326BD158C34109B9A77FB4E2BAC533948F`. These are particular stopped UI states, not arbitrary later dynamic controls or unpinned mod support.

The process exited normally, then GDB quit. Its generated GHM settings were preserved privately and the exact GHM prelaunch settings restored. EXE, both native Lua files, vanilla settings, GHM settings and all 12 existing saves matched (**17/17; zero mismatches**). No save command/fixture or timing run was performed. Stable GHM log SHA-256 `284F86408B90FC8C5CDDD952FEACA9081BD0A439B122E6AB09C35ACD723A6729`; preservation reports `native-GHM/identities-after.json` and `mod-preservation.json`. No owned game/debugger remains live after this experiment.

## Native outer dispatcher and borrowed Budget sample

A further original-Lua vanilla run used PID **33052**, GDB handle **26463**, actual base `0x210000`. Fresh Belgium remained paused on January 1, 1836; no save was loaded or written. Exact observed stack returns, rather than GDB's heuristic backtrace, identify the normal frame chain: rooted native `0x254D80` calls `0x248460` at `0x256339`, return `0x25633E`; that parent calls the idler virtual slot +0x4C at `0x24850F`, return `0x248511`, reaching `0x269B60`. The independent callback lane decoded these boundaries from rooted CFGs. Initial thread 1 was the executing thread.

Owner `0x47990FE8` had context at +0x54 equal to `0x2313B388`, matching the native global at base+`0xE58A80`. The context deletion flag +0x239 was zero. A minimap sample at the outer virtual +0x198 boundary `0x269CF7` / return `0x269CF9` retained element `0x4F0DC078`, descriptor `0x233E2EB0`, its A3CACC vptr, inline `minimap_pic` string and descriptor+0x68 context owner. This is a separate sampled element, not Budget evidence. A stale-hover topbar `text_budget` sample used descriptor subtype A41EE0 and descriptor+0x68 zero; that field is **excluded** from the two-type ownership inference.

After the UI queued actions completed, the actual Budget income element was `0x4E9B1A08`, vptr RVA `0xA38738`; descriptor `0x231FF088`, vptr RVA `0xA41D20`, inline key `tax_0_inc`, size9/capacity15. Its descriptor+0x68 matched context; context+0x98 matched element. Native Budget original target `0x206780` received ECX `0x4E995258`, Budget vptr RVA `0xA059F0`, output `0x1AD70C` and that same element. Explicit return trap `0x26A093` showed ESP `0x1AD6DC` -> `0x1AD6E8`, EAX output, EBX/ESI/EDI/EBP preserved, native CW `0x007F` and MXCSR `0x1FA1` unchanged. Element/descriptor pointers, both vptrs, inline key bytes, context owner and hover root remained unchanged immediately after original and at outer return `0x269CF9`. This directly supports the sampled immediate borrowed-read interval; it does not establish universal teardown exclusion or certify compiled fork calls. These native control values are not the frozen fork's effective settings.

A conditional context-destructor breakpoint at `0x5A3030` checked its proven stack argument against this context. Normal Alt+F4 shutdown exited without hitting it, so **no destructor ordering observation** is claimed. The process exited normally, GDB then quit, generated settings were retained privately and exact prelaunch settings restored. All 17 identities matched: executable, both native Lua DLLs, vanilla/GHM settings and 12 existing saves. No owned game/debugger remained. Stable private `native-lifetime/gdb.log` SHA-256 **EDEBA79D4CD8A7EDAC6291866E0E4860F118E6F0D9EC54FDF26FAD3A4334497F**; preservation `native-lifetime/identities-after.json`. No candidate DLL or HPM was launched.

The intended prospective workload is now HPM 0.4.6.3. The prior GHM evidence remains historical GHM evidence. Root's static HPM interface inventory (20 files, 6 GUI files) finds the four custom keys absent, but HPM overrides Budget/Politics and has no runtime receiver proof yet. Private inventory SHA-256 `725B67B3518400325FB6DF8AE066651BAECD5B500A63119ECEAA2D02B4BDCF4E`. S01HPM is the required independent native baseline after this checkpoint.

Debugger-created NTDLL DbgBreakPoint stops on threads 62/63/64 were manual interrupt mechanics, not GUI callback or teardown evidence. GUI thread claims above refer only to the explicit native breakpoint hits on original thread1. Heuristic GDB backtraces and wrongly aligned disassembly are excluded in favor of exact stack returns and independently rooted CFG boundaries.

## Retained-element capture pending raw-log review and cleanup

A subsequent original-Lua vanilla experiment is still live under PID31912/GDB90104. Prelaunch17/17 identities matched, base0x210000 was verified, Belgium was created fresh and remained pausedJanuary1,1836. The user stopped computer control before normal closure and explicitly authorized background work. **No further native-process interaction or cleanup has occurred after that stop.** Root found the active private `native-retained-element/gdb.log` still0bytes, consistent with buffering; the following facts come from tool outputs and require raw-log verification after an authorized flush/close. No final preservation/normalexit claim is made.

The tool-output capture shows Budget original206780 with view57E5C658/vptrA059F0, element57E79098/vptrA38738, descriptor1ECEF088/vptrA41D20, `tax_0_inc`9/F, return26A093 and output1AD70C. Native original nonvolatiles were preserved; CW007F, MXCSR1FA1, fstat4024 and ftagFFFF were observed unchanged at originalentry/return and the later captured stops. These are native diagnostic observations, not frozen fork effective settings or a full machine-invariance requirement.

At outer return269CF9, localbaseP=ESP1AD9F8 and retained[P+0C] at1ADA04 equaled the EXACT passed element57E79098. The naturally executed native269D37 stop had ECX equal to that element, EDX450B40 (base+240B40), same retainedlocal, elementvptr and descriptor/name. The branch's compositefield[P+70] was0; no primary-tooltip semantics are assigned to that field. Thus the existing native continuation actually dereferenced the retained element's vptr after originaldispatch. A second activation captured269CF7 with ESP1AD9F0, passed[ESP+4] and retained[ESP+14] both sameelement, then269CF9 with [ESP+0C] unchanged. No native call was injected.

Static rooted alias/continuation evidence is independently described in the callback-lifetime report. It establishes the native borrowing expectation for this path, rather than demanding re-certification of every unchanged vanilla formatting/arithmetic callee. The observed sample still awaits raw-log review. At the stop, process was continued/running after the final return; prelaunch settings backup `native-retained-element/settings-before.txt` exists. Generatedsettings retention, exactrestore and post-exit identities remain pending explicit computer approval. No HPM or candidate was launched.

## Retained-element recovery and accepted closed evidence

The user renewed computer-use authorization. Read-only recovery at about22:37EDT found no v2game.exe (including PID31912) and no Victoria2 window; the existing GDB7172 log ended `[Inferior 1 (process 31912) exited normally]`. The exact exit time and user interaction during the unattended interval were not observed. The recovering manager did not initiate that game closure, launch a replacement, attach another debugger or issue any UI input.

The replacement manager and root each received `Unknown process id90104` from their PTY capability; neither inferred target exit from that error. Only the original manager retained the existing handle. Root transferred an exclusive debugger-only recovery lease to that owner after those two failures. The original handle returned the normal inferior exit; `info inferiors` showed `<null>`, logging was disabled, then `quit` exited0. The lease returned to the current manager; subsequent OS inventory found no owned game/GDB. No restart/kill or game/desktop manipulation occurred.

Stable private `native-retained-element/gdb.log` SHA-256 **`E5260F5BC81BDE71231811EFE378045799E095DBA19C90E7BD4BF16D3206AF90`**. Root independently read/hash-verified the actual entry/return, retainedP+0C and same-element+9C dereference, descriptor/name/control-state samples, following activation and normal exit. The raw retained-element sample is now accepted at that scoped native borrow boundary; earlier pending statements are historical, not its current state.

Current settings were preserved first as `settings-current-at-resume.txt`, then generated settings as `native-generated-settings.txt`. Both and the original `settings-before.txt` have SHA-256 `107719DA564543731EF6B1F13120F32F710ADCF0367044DE303CE4F1B6AA87EA`; there was no config conflict or need to overwrite already-exact bytes. The post-exit17-file comparison covers EXE, both native Lua files, vanilla/GHM settings and12existing saves: **17matches/0mismatches**, independently rechecked by root. Reference identities are the accepted previous stable `native-lifetime/identities-after.json`; prelaunch17matches were previously captured in tool evidence, and no nonexistent prelaunch JSON is invented. Final report `native-retained-element/identities-after.json` SHA-256 **`5824F99810D84603D6DAA334820EE960933FE5233F0FE26FC754DD6EE872CDE0`**.

No save command, candidate load, HPM runtime, requested-feature acceptance or timing run occurred. This closes this experiment's evidence/preservation, not the production family or complete active profile. Future test windows should use the user's second monitor, avoiding the monitor occupied by the user's game; recovery launched/moved no window.
