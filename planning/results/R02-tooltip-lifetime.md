# R02 immediate tooltip borrowing and lifetime evidence

Observed 2026-10-17; static/source worker only. No process/debugger control, engine call, deployment or shared-source edit. This investigation narrows the **active pinned vanilla/GHM tooltip path**; the generic custom-observer path remains unsupported and is not reintroduced as a mandatory active path after the manager's actual receiver/list/root observations. Runtime observations are credited to [the manager's report](R02-runtime-views.md); native slot/lookup evidence is in [the PE report](R02-native-views.md).

Latest observation state: after explicit renewed computer-use authorization, recovery found PID31912 already exited normally. The original debugger owner recovered handle90104, verified no inferior, disabled logging and quit with exit0. All17preservation identities matched; settings already equaled their exact backup and were preserved without overwrite. Root independently accepted the closed log and retained-element sample. Stable log SHA-256 `E5260F5BC81BDE71231811EFE378045799E095DBA19C90E7BD4BF16D3206AF90`. Earlier pending-state sections below are chronological checkpoints, superseded only for this experiment's recovery/preservation. This worker performed no live control; ownership/recovery is in the manager's runtime report.

## Reproduction and identity

Read-only tool `tools/verify/r02_tooltip_lifetime.py` rejects every executable except SHA-256 `62D48C204364DD706584777C2E2B3C7AB3C5F1DD0170872554943575D53D6648` and refuses evidence output beneath the repository. It uses pefile 2024.8.26 / Capstone 5.0.7. Exact executable excerpts remain private at `%LOCALAPPDATA%/V2DLLFork/R02/tooltip-lifetime/probe-final.json`, SHA-256 `16ECEAB5E5F4324F0ECD3EF14CC79C563B6AF857E395DB3952FF0696910351F1`.

Initial tool SHA-256 `62EEBBBAB075271B5F7B261BA1A4B9DA7BC464DADF95F0B9D188208D97778291`; syntax AST check passed. A four-byte unknown fixture exited1 with the explicit identity rejection and no output; an attempted repository-local evidence destination exited1 with the explicit destination rejection and no output. Successful pinned run exited0, produced three verified direct caller candidates, and wrote only private evidence. Report/tool UTF-8 and trailing-whitespace checks passed. Later expanded ownership tool/evidence identities are recorded below; the initial private artifact is retained.

```powershell
$env:PYTHONPATH = Join-Path $env:LOCALAPPDATA 'V2DLLFork/S02/python'
python tools/verify/r02_tooltip_lifetime.py --exe 'C:/Program Files (x86)/Steam/steamapps/common/Victoria 2/v2game.exe' --output "$env:LOCALAPPDATA/V2DLLFork/R02/tooltip-lifetime/probe-final.json"
```

The tool follows direct CFG branches without entering callees; it does not infer exception edges or resolve arbitrary virtual calls. A byte-position E8/E9 target scan proposes direct callers; only calls reached at five-byte instruction boundaries from a recorded forward CFG are accepted. The nearest conventional prologue remains an entry lead, not an exhaustive function-start or indirect-caller proof.

## Native caller and post-return interval

Entry RVA `0x26A040` receives selected-view owner in EAX, output storage in EDI and element as its one explicit stack argument. On its selected-view branch it pushes element, pushes output, and calls the selected view's slot `+0x28` at `0x26A091`. This is the naturally observed call site for all three tooltip originals. The tooltip's ret8 consumes those two arguments.

Immediately after return (`0x26A093`), native code moves EDI to EAX, restores ESI/EBX/frame and returns ret4. There is **no element reread, retain/release, destructor call or other call in that immediate post-return interval**. The dispatcher does not take an explicit element reference before the call either. Thus caller argument forwarding proves the value, but not an additional lifetime extension for the fork's post-original getter. Absence of an immediate release is useful evidence; it is not proof that the original or one of its callees cannot invalidate the element/descriptor.

Three direct call candidates to this dispatcher were found and independently reached from forward CFGs:

| Dispatcher call | Parent entry lead / CFG instruction count | Arguments and output lifetime immediately around call |
| --- | --- | --- |
| `0x30A1E7` | `0x309910` / 784 | Push ESI element; EDI points at local output `[ebp-0x11C]`; EAX takes owner from `[ebp+8]`. After return, pass result to helper `0x115D0`, then local temporary to `0x59820` |
| `0x30AB36` | `0x30A340` / 2581 | Push parent argument `[ebp+0xC]` as element; supply local output through EDI; restore owner from stack local into EAX. Output joins common processing at `0x30C4CF` |
| `0x30C874` | Same parent / same CFG | Push current EDI as element, replace EDI with local output, owner from stack local into EAX; same common result processing |

All parent CFGs have no unresolved direct jumps, decode/nonexecutable stops or exhausted budget. At common `0x30C4CF`, returned EAX is passed through EDI to `0x115D0`, then the local output goes to `0x59820`; no direct element release occurs in that immediate continuation. Their callee ownership contracts, all branch paths and indirect event dispatch remain unproved. This scan covers direct E8/E9 targets only; it does not establish the complete caller set or identify an OS event-loop exclusion rule.

The source does not retain the tooltip name across a tick. It does retain the incoming element across the original call in its own activation, then calls GetName afterward. Stack-local output storage remains live until the parent's later processing, and the native original returns the supplied output pointer. Those facts support the concrete output object's scope, but do not establish every internal string's lifetime or absence of reentrant mutation.

## Concrete getter contract: bounded and read-only

For the observed element vtables `0xA38738` and `0xA3A930`, slot `+0x44` resolves to `0x2465D0`. This is exactly four instructions: read descriptor at element+`0x30` into ECX, read descriptor vptr, load its slot `+0x18` into EDX, tail-jump. For observed descriptor vtables `0xA41D20` and `0xA3CACC`, that slot resolves to `0x5ED1D0`: return descriptor+8 in EAX, plain ret. These static target/relocation identities were already independently established in the PE lane and matched the manager's sampled objects.

For this **specific six-instruction composed chain**, with both concrete vptr identities held valid throughout:

- Incoming ECX is element; no stack arguments or hidden return buffer are consumed; ordinary ret returns the borrowed GStr address in EAX.
- ECX becomes descriptor, EDX becomes the final getter address, and EAX becomes descriptor+8. EBX/ESI/EDI/EBP and stack argument space are untouched.
- There are no writes, calls to external helpers, allocations, reference-count changes, branches into a message loop or arbitrary callback calls after the concrete tail target is fixed.
- MOV/LEA/JMP/RET neither alter EFLAGS nor execute floating-point/SIMD instructions. This exact chain preserves x87 stack/control/status and all XMM/MXCSR state. This bounded instruction fact is stronger than merely observing a control word unchanged.

The bare getter CFG records its indirect jump as unresolved; the composition closes it only for the two descriptor-vptr/slot observations above. The profile must reject unknown element/descriptor implementations rather than applying this result to every GUI subtype. Runtime type validity and readable object lifetime are still preconditions, not outcomes of the instruction proof.

The returned object is the descriptor's GStr beginning at+8. Its size lies at descriptor+`0x18`, capacity at+`0x1C`; capacity15 selects inline bytes, greater capacity selects the data pointer stored at descriptor+8. Source GStrText then borrows this memory for strcmp. It does not acquire ownership, lock a descriptor, bounds-check size or retain the result. String byte reads and CRT strcmp are separate compiled boundaries beyond the six native getter instructions.

## Construction and destruction evidence

| Native entry | Relevant local evidence | Narrow implication |
| --- | --- | --- |
| Descriptor creation `0x658D40` (45 instructions) | Writes vptr `0xA41D20` at `0x658D72`; initializes name inline NUL at descriptor+8, size0 at+`0x18`, capacity15 at+`0x1C` | Supports observed descriptor name layout from initialization, not only a returned address |
| Descriptor creation `0x5ECA80` (56 instructions) | Writes vptr `0xA3CACC` at `0x5ECAD5`; initializes same name offsets/representation | Independently supports second observed descriptor type |
| Constructor-derived `0x2466D0` (57 instructions) | Temporarily writes `0xA3CACC`, later overwrites primary vptr with `0xA08AF4` at `0x24679A` | A matching constructor write alone cannot certify final dynamic subtype; this path is not substituted for the actual observed A3CACC object |
| Common element construction `0x5A6A30` (50 instructions) | Stores incoming EDX into element+`0x30` at `0x5A6A70`; does not clone descriptor or increment a visible descriptor refcount | Element carries a borrowed native descriptor pointer |
| Text-element construction `0x5A6430` (147 instructions) | Calls common construction then writes observed element vptr A38738 | Connects concrete element table to base descriptor field |
| Button-base construction `0x5EE0B0` (42 instructions), production-element creation `0x5C3C70` (84) | First forwards through common construction; second calls button-base construction and writes observed A3A930 | Independently connects second observed element table to same descriptor field |
| A3CACC scalar deletion `0x5ECB60` (24), common descriptor destruction `0x2467C0` (31) | Deletion calls common cleanup; if name capacity>=16 common cleanup passes the name's heap pointer to native release entry `0x6AE91B`, then clears size, resets cap15 and writes NUL | Descriptor/name memory has an explicit end-of-life path; getter does not make it immortal |
| A41D20 scalar deletion `0x658E00` (15), derived cleanup `0x658E30` (35) | Cleanup eventually invokes same common descriptor cleanup; scalar deletion may then release descriptor itself depending on flag | Same lifetime constraint applies to the first descriptor type |
| Text-element teardown `0x5A6660` (71), button teardown `0x5C41D0` (47) / `0x5C2F40` (41) | Reset element vptrs and release local owned fields; button path removes its observable state through another helper. No direct descriptor+`0x30` release appears in these traversed bodies | Supports externally managed descriptor ownership, but transitive cleanup/definition-store owner and teardown ordering are not proved |

All tabulated direct CFGs have no unresolved jumps, decode stops or exhausted budget. Callees and exception-cleanup edges are not thereby certified. Destruction evidence deliberately does not infer the definition-store owner's identity or full descriptor lifetime from the absence of a direct free in element teardown.

## Phase, reentrancy and FPU accounting

Manager samples ran on the initial game thread for the specific native tooltip paths, with native x87 control/MXCSR observations unchanged across each original. The same-thread call sequence plus read-only six-instruction getter cannot itself enqueue teardown or pump messages. This establishes a useful narrow immediate-read contract once objects are live. It does **not** exclude another thread, nested event dispatch inside an original/helper, GUI reload/destruction, descriptor replacement or other callback reentrancy during the earlier original call.

On the pinned supported path, native child searches miss the custom controls and actual fallback roots are zero, so source setup does not register static clones or invoke the deferred production refresh. The immediate setup still calls lookup helpers before the original and uses shared 128-byte name storage with capacity127. Known native search bodies copy/consume keys synchronously; main-thread samples alone do not prove exclusion of reentrant shared-buffer reuse. The full generic clone lifetime is not required to interpret these pinned observations and remains separately unsupported.

The native getter chain has full instruction-level floating-state preservation as described above. Wrapper/setup/CRT/compiler helpers need the actual x86 nonvolatile, stack, floating-control and return obligations; arbitrary volatile EAX/ECX/EDX/XMM equality is **not** required without an actual caller-liveness reason. Source pushad is not itself floating-control preservation, and native control-word equality samples do not certify compiled fork helpers, all lookup/import callees or exceptional returns. Frozen FPU behavior/settings remain unchanged by this work.

## Result and exact next unknown

**Established:** exact selected-view forwarding shape; three direct parent-call boundaries and stack output scope; two concrete descriptor initialization/name layouts; direct element descriptor-pointer storage; exact six-instruction getter's register/stack/flags/full floating-state contract; native descriptor-name destruction path. This materially narrows the borrowed-name proof.

**Still unproved:** exclusion of element/descriptor teardown or replacement during the native original and until post-original GetName/strcmp completes; the externally owning GUI definition store's teardown order; complete caller/event-loop and reentrancy/thread exclusion; compiled fork-helper/CRT floating/register behavior. The dispatcher contains no retain/release lifetime barrier that closes these questions. Do not label a source-local pointer or observed unchanged sample a general lifetime guarantee.

The next bounded hypothesis is that UI element/descriptor destruction is serialized by the same native GUI event dispatcher and cannot execute during these tooltip activations. Prove the dispatcher's ownership/teardown ordering (including reload/resign) or supply explicit guards/constraints in the eventual supported profile. A native breakpoint comparison of the passed element, element+0x30, both vptrs and descriptor string before/after original return would verify selected samples but still needs that structural ordering evidence. At this initial checkpoint no new live experiment had been requested; the later bounded request and manager-owned evidence are distinguished below. No candidate/profile is enabled.

## Deeper hover-owner and definition-store trace

Continuation within R02, same worker ownership. The intended workload was subsequently corrected by the user to **HPM**; earlier GHM observations are historical. The native executable facts below are independent of a mod; this report makes no HPM asset/runtime claim. The manager was given one exact optional native observation boundary while this static lane continued; this worker never controls that process.

Expanded probe `probe-ownership.json` in the same private folder has SHA-256 `E4EFBC08147A5F8DD6EE4F28131CE802F6D4EFF9540B8071F37266AFC8FCB6A1`. Tool version SHA-256 `F84463002B56AD56E542CC37EE08710A7972B5E8E3885744BEB3D6041D8C4654` adds34 explicitly listed ownership/creation/teardown/frame CFGs plus the bounded pinned factory switch and three specific descriptor-name targets. Reproduce with the command above using output filename `probe-ownership.json`. Earlier artifacts remain historical snapshots, not the output claimed for the expanded script.

### Exact hovered-element origin

RTTI before vtable `0xA0A34C` names `CInGameIdler`. Its slot `+0x198` (`0xA0A4E4`) points to the rooted tooltip parent `0x30A340`; slot `+0x4C` points to `0x269B60`. The latter's 305-instruction CFG has no unresolved jumps or exhausted budget. Its native dataflow is:

1. Save ECX idler into EBX. At `0x269CC9`, load GUI context from idler+`0x54`; at `0x269CCC`, load current hovered element from context+`0x98`; preserve it in a local and idler+`0x160`.
2. If non-null, read idler vptr and its `+0x198` method, push that element and a stack-local output object, and call at `0x269CF7`. With the observed CInGameIdler type, that method is `0x30A340`.
3. `0x30A340` either directly forwards its second stack argument to selected-view dispatcher `0x26A040`, or calls helper `0x309910` at `0x30C57F`. That call sets ECX to the same element from `[ebp+0xC]`, supplies owner and output as stack arguments. Helper `0x309910` retains incoming ECX in ESI; its dispatcher call pushes that ESI. The fallback call at `0x30C874` likewise forwards the active element through EDI on its reached branch.
4. The original view callback therefore receives an element borrowed from the GUI context's hover state, not an independently owned object created for the tooltip call. The native parent stack output is constructed/consumed/destroyed separately.

An analogous base/UI tooltip getter `0x1FB7E0` (139 instructions, no unresolved jumps) reads its context via owner+`0x98`, then hovered element via context+`0x98`, and dispatches owner `+0x198` at `0x1FB8B8`. This is a distinct receiver layout, not substituted for the exact in-game path above.

Element teardown explicitly invalidates the shared hover pointer: text teardown `0x5A6660` loads context from element+`0x34`, compares `[context+0x98]` with the dying element at `0x5A6716` and clears it at `0x5A671E`. Button-base teardown `0x5C2F40` performs the same comparison/clear at `0x5C2F88` / `0x5C2F90`. This is concrete native weak-pointer invalidation, rather than an inferred refcount. The existing local tooltip activation can still hold the old value if teardown reenters during its call, so the clearing alone is not the exclusion barrier.

### Exact GUI definition owner and destruction order

The common context constructor `0x5A2EC0` (77 instructions) writes primary vptr `0xA385C8`, parser subobject vptr `0xA38660` at context+`0x0C`, embedded definition-tree vptr `0xA41E84` at+`0x34`, clears its tree root+`0x38` and hovered element+`0x98`, and publishes context in global RVA `0xE58A80` at `0x5A2FD5`. Its primary vptr was **not assigned a class name from a guessed RTTI entry**.

The parser subobject's slot `+8` is the factory `0x5A41C0`. Its switch is now bounded directly: input is reduced by `0x7C`, compared unsigned against `0xFA`, byte-map `0x5A4508` maps those 251 accepted indices to0..15, and jump table `0x5A44C8` selects16 targets. Including those targets yields224 reached instructions and no unresolved jumps. The A41D20 branch at `0x5A428B` allocates0xE4 bytes, obtains constructor owner=context by subtracting0x0C from parser-this, then calls `0x658D40` at `0x5A42A8`. The A3CACC branch at `0x5A43CF` similarly calls `0x5ECA80` at `0x5A43F5` with context in EDX. Both constructors store that borrowed definition-store owner in descriptor+`0x68`; descriptor+`0x64` takes context+`0x30`.

Parser orchestration `0x5A4990` (77 instructions) invokes factory subobject slot+8 at `0x5A4A0D`, parses the resulting descriptor, obtains its name through descriptor slot+`0x18` at `0x5A4A2C`, and passes the descriptor pointer plus name to the supplied tree's slot+`0x18` at `0x5A4A35`. Context parser adapter `0x5A4150` (25 instructions) supplies context+`0x34` as that tree and context+`0x0C` as factory. Thus this is an owning GUI definition tree, with descriptor creation/registration separate from element creation. These two CFGs have no unresolved direct jumps. Unknown virtual parser implementations and two other unused factory leads remain unresolved in the private exploratory artifact and are not promoted to this specific path's evidence.

Context teardown `0x5A3030` (189 instructions) establishes the actual destruction sequence:

1. Set context+`0x239` to1 at `0x5A306A`.
2. Iterate pointers in vector context+`0x5C/+0x60`, invoking each member's virtual `+0x30` at `0x5A308B`.
3. Traverse definition tree rooted at context+`0x38`. At `0x5A30BD`, invoke each non-null node value's deleting virtual slot0 with flag1; recursion helper `0x5A4A60` (41 instructions) does the same for child/sibling nodes at `0x5A4A89`.
4. Iterate vector members in reverse, invoking virtual `+0x14` with flag1 at `0x5A3107`; then clear/delete tree storage and other context-owned collections.
5. Clear global RVA E58A80 at `0x5A317A`; reset context+`0x239` to0 and continue member cleanup.

All these CFGs have no unresolved direct jumps or exhausted budget. No method names or thread exclusion semantics are inferred solely from the two flag values. The definition-owner tree and its deletion transition are now explicit. The sampled descriptor's owner can be checked directly as descriptor+`0x68` against idler+`0x54` and global E58A80; the current report has not invented that runtime equality before observation.

The destructor's exact context argument is `[entry ESP+4]`: it loads ESI from `[ebp+8]` at `0x5A304B` and normally returns ret4 at `0x5A3265`. Scalar-deleting wrapper `0x5A3000` instead receives context in ECX, pushes it, calls the destructor at `0x5A3007`, and conditionally frees the allocation later at `0x5A3013`. The explicit return `0x5A300C` is after member/definition teardown but **before** that optional native free; context vptr has then changed toA38670 and parser vptr toA38318. A teardown observation must use this exact ABI and avoid dereferencing the allocation after scalar deletion.

The idler itself borrows context: base construction `0x247D30` (57 instructions) stores its context stack argument into owner+`0x54`; in-game construction `0x24A6D0` calls it and installs CInGameIdler vptr. In-game destruction `0x24C5C0` tears down its own members and calls base cleanup, without directly destroying owner+`0x54` in that body. This distinguishes campaign-view recreation from destruction of the shared definition store, although transitive event ordering still needs the outer dispatcher boundary.

### Remaining bounded exclusion question

The exact missing transition is now **outer caller of CInGameIdler slot+0x4C -> hovered-element/context activation -> definition/element teardown**, rather than an unspecified owner. Neither `0x269B60`, `0x30A340` nor `0x26A040` visibly acquires a lock/refcount or tests context+`0x239` around its tooltip dispatch. That does not imply a native bug: serial native event dispatch may provide the required barrier. It does mean this static chain alone has not demonstrated it.

The manager received a precise original-Lua capture request: at entry `0x269B60`, record exact `[ESP]` outer return, ECX/idler vptr, context+hover/flags/global and element/descriptor/vptr/owner; at before-call `0x269CF7` and explicit return `0x269CF9`, compare those identities; conditionally observe context destructor `0x5A3030` for that same owner. An actual outer return allows its exact forward CFG to establish event dispatch/teardown order without treating every `+0x4C` method in the executable as a possible required dependency. No injected native call or guessed GDB finish is needed. Structural proof can then be limited to the actual observed dispatcher, with other unsupported callers failing closed.

### Manager's exact outer-frame observations and independent rooting

The manager subsequently reported original-Lua PID33052, mapped base0x210000, initial game thread. The exact return captured from entry ESP at `0x269B60` was VA0x458511, hence RVA`0x248511`. A heuristic backtrace suggested a different ancestor and was **not** used as that immediate return. Static forward CFG from entry `0x248460` reaches call `0x24850F` through idler slot+`0x4C`, with exactly that return. Its392 reached instructions have no unresolved jumps/stops/budget exhaustion.

At entry `0x248460`, the manager then captured exact stack return VA0x46633E, RVA`0x25633E`, owner0x47990FE8 and boolean1. Static entry `0x254D80`, CInGameIdler vtable slot+4, reaches five-byte call `0x256339`, returning exactly there. That frame CFG has2956 reached instructions and no unresolved direct jumps/stops/budget exhaustion. At the call site it pushes EBX/idler and the boolean produced by native helper `0x23F7F0`. The exact active ancestry is therefore:

`CInGameIdler frame 0x254D80 -> 0x248460 -> 0x269B60 -> 0x30A340 -> optional 0x309910 / selected-view 0x26A040`.

The manager's first before/after269CF7/269CF9 sample had GUI context0x2313B388 equal both idler+`0x54` and globalE58A80; minimap element0x4F0DC078, descriptor0x233E2EB0 with vptrA3CACC and descriptor+`0x68` equal that context. Hover root, name string header and descriptor identity were unchanged, with context+`0x239` zero. This directly corroborates that sampled definition-owner relation. It is a minimap parent-dispatch sample, not a substitute for the selected-view VIEWS callback return interval; manager-owned raw evidence/preservation belongs in the runtime report.

The manager subsequently captured the concrete **Budget selected-view** path in that same process/thread: at269CF7 the hovered income element was0x4E9B1A08 with vptrA38738, descriptor0x231FF088 with vptrA41D20, inline key `tax_0_inc` size9/cap15; descriptor+68 and global both matched context0x2313B388, context+98 matched the element. Original entry206780 used Budget view0x4E995258 with vptrA059F0, output0x1AD70C, the same element argument and exact return VA0x47A093 (RVA26A093). The explicit native return preserved nonvolatiles, CW0x007F and MXCSR0x1FA1; element, descriptor, vptrs, name bytes, owner/global/hover root were unchanged and context+239 remained0. The next outer return269CF9 was reached. This verifies a real original-return insertion point for the bounded read-only post-getter chain, with structurally corroborated owner and unchanged identity; it is no claim about unobserved teardown/reentrant paths or a loaded fork wrapper.

A separate topbar sample had descriptor vptrA41EE0 and descriptor+`0x68` zero. It is deliberately **excluded** from the A41D20/A3CACC owner-field rule. Static tableA41EE0 slot+`0x18` still resolves to the same two-instruction name getter5ED1D0, so the composed getter's register/stack/control contract applies if its actual element getter is2465D0; no additional ownership fields are assumed for that subtype.

At the actual outer frame's immediate post-tooltip continuation, `0x248527` reloads XMM0 from memory and `0x24852F` reloads XMM1; its ensuing arithmetic does not require those two volatile values to survive the tooltip call. EDI/ESI/EBX/EBP/stack and floating-control obligations remain. The reached direct IAT calls contain no PeekMessage/DispatchMessage/SendMessage in248460 or269B60, and only PostQuitMessage in254D80 at256C30. This excludes direct message pumping in those three bodies; it does not assert every transitive native helper is nonreentrant or infer a lifetime lock from address order.

Remaining structural gate is now narrowly scoped: native selected-view tooltip callees must not invoke hovered-element/GUI-definition deletion or reenter the GUI teardown dispatcher before returning to its post-original getter. Exact teardown sites, shared owner, outer frame and invalidation store are identified above. The actual frame stack plus synchronous read-only getter materially supports normal serialized UI borrowing; a universal cross-thread/reload guarantee is not claimed from sampled frames alone.

## Native observation closure and bounded transitive audit

PID33052 exited normally after Alt+F4. The manager's conditional context-destructor breakpoint **did not hit**; no native context teardown ordering was observed. Static destructor facts above remain static facts and are not relabeled observed. Private `native-lifetime/gdb.log` SHA-256 is `EDEBA79D4CD8A7EDAC6291866E0E4860F118E6F0D9EC54FDF26FAD3A4334497F`; the manager reported17/17 preserved identities after normal closure. This worker never controlled that process. A user-requested computer-use pause interrupted final audit-report reconciliation; the same R02 lane resumed only after renewed authorization.

Audit reader version SHA-256 `F8060373882D5A4C5BD55B39C2BF184FF5F0C3574B5463CF9CF9F61562E2F5C2` generated the retained private `probe-call-audit-depth6.json`, SHA-256 `D9122C1ACCF31300D560E25CD9CFAC913487B40CA07096A85A4A5423C5188C68`, independently rechecked after resume. Earlier reader hashes describe their earlier retained artifacts. The later current reader/artifact below include the retained-local helpers and explicit frontier distinct counts.

| Original | Complete direct CFG instructions | Direct callsites / unique direct targets | Indirect callsites |
| --- | --- | --- | --- |
| Budget206780 | 4809 | 815 / 91 | 40 |
| Production3010A0 | 1838 | 250 / 41 | 34 |
| Politics2D2DD0 | 4049 | 638 / 73 | 87 |

The audit expands direct calls only, with depth limit6, body limit750 and instruction limit200000. Actual result: **741 bodies,90698 decoded instructions,37 depth-frontier records representing33 distinct targets,31 distinct targets still unvisited,448 indirect callsites,9 bodies with unresolved indirect CFG jumps**. Two recorded frontier targets were covered later; records are not37 distinct outstanding functions. Neither global budget was exhausted; all37 records arose from the depth bound. A prior depth3 checkpoint remains private as `probe-call-audit.json`, SHA-256 `98D973A18F5149A23F5B725417191BE6A469566142B96E2CC6775C482D40661D` (437 bodies/72824 instructions/236 frontier/374 indirect/6 unresolved CFG bodies).

No reached known context, element, descriptor teardown or frame-reentry target appears among these resolved direct calls. Resolved imports contain allocator/runtime/error handling and GetSystemMetrics, with no GUI/message-pump import. This is a bounded **negative result**, not a proof that no teardown/reentrancy is possible: unknown virtual targets, unresolved switch tails and the explicit frontier remain exclusions. Normal/error/runtime paths are included together; allocator failure/exception handling is not promoted into an observed normal tooltip path.

The unresolved CFG bodies are6B1550,6AE480,5AA7C0,6B9DF1,732BDC,0EFAD0,732887,6B7BEA and577100. Concrete engine-side paths include Budget206780->5A8EE0->5ACD20->5AA7C0; Production3010A0->2FE7F0->0F2040->0EFAD0; Politics2D2DD0->11E410->11D140->0B9CC0->0BD400->577100. They remain unresolved instruction-control edges; no semantic teardown assertion is attached merely to their presence.

One apparent deletion edge is explicit and deserves receiver classification: Production original call3017DC invokes virtual slot0 with flag1, but its ECX receiver comes from global RVA E5B5E0 (the existing native options singleton used by `OptionsMusicVolumePtr`), following a conditional allocation/constructor75150 sequence. It is **not sourced from the tooltip element argument**. Concrete constructor/vptr/ownership evidence is still needed before calling this a harmless options-only deletion; conversely, it must not be mislabeled observed hovered-element deletion.

Budget's widget+54 slot+10 sites likewise must not be called observer notifications by analogy with a different observable table. Calls20ACE4/20AF53 immediately consume ST0 with FSTP; call2072FA follows with FDIV. The existing source's native scrollbar GetValue interface documents the same+54/+10 float-return shape. This corroborates numeric getter use, but actual target identity and absence of callbacks still need the concrete scrollbar subtype; a source comment alone is not native proof.

## Native retained-element continuation: stronger borrow evidence

This continuation supersedes reliance on the bounded absence audit for the **native tooltip interface's immediate element borrowing expectation**. It does not turn that audit into a complete call graph or certify unknown custom GUI implementations.

Current reader SHA-256 `0A42FC2942BCCC3C92A0A62B53BDC70030B69371102DFAFD05F815F527FD30AF`. Exact current reproduction:

```powershell
$env:PYTHONPATH = Join-Path $env:LOCALAPPDATA 'V2DLLFork/S02/python'
python tools/verify/r02_tooltip_lifetime.py --exe 'C:/Program Files (x86)/Steam/steamapps/common/Victoria 2/v2game.exe' --output "$env:LOCALAPPDATA/V2DLLFork/R02/tooltip-lifetime/probe-call-audit-current.json" --audit-calls --audit-depth 6
```

Current artifact SHA-256 `3D3B02DFB4D8B5DDDAAB73AE892F94150ED48F4E8BB98E8B938265BFC405572E`; it includes38 focused CFGs, concrete retained-element virtual slots, and the unchanged741-body audit with explicit37/33/31 frontier counts. Retained-local checkpoint `probe-retained-element.json`, SHA-256 `6A98D4473246E6AED4C7E59488E070BD3618B12329DBD6D8B271716453DD110E`, remains private and unchanged.

Let P be269B60's ESP immediately before it pushes its two native dispatch arguments. At269CD2 native code stores the hovered element in `[P+0x0C]`. It then passes that same element to virtual+198 at269CF7; after the callee's ret8, ESP=P at269CF9. This is the same outer activation as the exact native selected-view chain, including206780/3010A0/2D2DD0 through26A040.

After269CF9, native code copies/destroys temporary tooltip storage, then tests the unnamed composite output field `[P+0x70]` at269D20. Do **not** call this primary-tooltip emptiness: it is an internal composite field whose semantic name is not established here.

| Reached branch | Exact native retained-pointer behavior |
| --- | --- |
| Field zero |269D2B loads ECX from `[P+0x0C]`;269D2F dereferences that element's vptr;269D31 loads its+9C method;269D37 calls it. There is no fresh hover-root reread, resolver or validity test |
| Field nonzero |269D25 branches directly to269E21, which reloads EAX from the same retained local;269E3D publishes it to global RVA FF0888. The zero-field branch eventually joins this continuation too |

For the concrete observed text-element vptrA38738 and production-element vptrA3A930, disk slot+9C targets240B40: exactly `mov eax,<relocated static empty string>; ret`. Thus the native post-original code **actually dereferences the previously passed element** even though this particular virtual method itself needs no descriptor. It is not a reread of a newly hovered element that could hide replacement/deletion of the original.

### Retained-local alias and stack check

The two intervening helpers do not receive the retained-local address:

- 269CFB sets ESI=P+28 and EDI to the native result at P+BC; helper115D0 (23 instructions) copies three native strings at offsets0,1C,38, then fields54/58. It writes destination P+28..P+80, a disjoint composite output object.
- String helper8ED0 (103 instructions) saves ECX destination in ESI, receives source/offset/count as three stack arguments, and all normal returns are ret0C. The copy calls supply position0/count-1. Visible writes update the destination string's size/data, growing its own storage if needed; it does not receive P+0C or a pointer to the parent stack frame. Source/result validity uses the existing native GStr invariants.
- 269D0C sets ESI=P+BC for destructor59820 (31 instructions). Its direct writes reset only the three temporary string headers/data; its native-release calls receive those strings' owned heap pointers and use explicit caller cleanup. The helper itself returns with plain ret.
- Outside those helpers, the only explicit parent-stack stores between269CF9 and the269D2B/269E21 fork update unwind state at P+124. No instruction overwrites P+0C or passes its address. The helper argument pushes/ret0C and native-release push/add4 balance; both paths reach the same base P before rereading P+0C.

These are ordinary valid-string/normal-ABI contracts, not a claim that corrupted native strings or arbitrary allocator hooks cannot scribble a caller's stack. They establish the required distinction between a retained input pointer and a fresh hover lookup.

### Bounded inference and minimal runtime corroboration

The existing native tooltip interface therefore expects its input GUI element to remain live across its own+198 dispatch and subsequent element virtual call. The unmodified selected-view originals execute inside that dispatch. This is a direct native **borrowed-input** contract, backed by an actual post-dispatch native dereference, rather than requiring every transitive economic/string routine to be formally proved non-deleting. Combined with the concrete Budget before/after identity and its descriptor-owner relation, it supports the immediate scoped post-original GetName read under the native object's valid descriptor invariant. The descriptor store relation and actual names remain subtype-specific; no raw pointer may be retained beyond that native activation on this evidence.

Minimal manager-owned corroboration uses no injected call: at269CF7 compare passed element `[ESP+4]` with retained local `[ESP+14]` (two arguments have already been pushed); at269CF9 verify the same pointer in `[ESP+0C]`; at269D2B/269D37 capture the retained value, its actual vptr and resolved+9C target; at269E21/269E3D compare publication of that same pointer. For observed A38738/A3A930 the target must be base+240B40. Observe the natural branch only; no requirement exists to force an empty tooltip or create a new native lifecycle test. The manager owns subsequent raw observations and preservation evidence.

This inference does not authorize unrelated deferred observers, raw cached view pointers across ticks, candidate deployment, unknown dynamic GUI plugins or calling from a simulation phase. Remaining production work includes supported active-profile gating, compiled adapter preservation, pinned supported UI/object subtypes and actual fork invocation evidence. No universal lifetime closure is claimed.

## Naturally executed retained-pointer capture, cleanup pending

The manager reported the following original-Lua observations in PID31912 under GDB session90104, mapped base0x210000. These are manager-owned live observations corroborating the static continuation above, not a fork invocation or a process this worker controlled.

| Boundary | Concrete observation |
| --- | --- |
| Budget original206780 | Element0x57E79098 with vptrA38738, descriptor0x1ECEF088 with vptrA41D20, inline key `tax_0_inc` size9/cap15; exact original return26A093; native nonvolatiles preserved |
| Return269CF9 | ESP=P=0x1AD9F8; retained local addressP+0C=0x1ADA04 contained the same element0x57E79098 |
| Naturally taken native call269D37 | ECX was the same retained element; EDX=0x450B40=base+240B40, matching the concrete native+9C target independently decoded above. Retained local, descriptor pointer/vptr and name were unchanged. Tested composite field `[P+70]` was0; no semantic name is assigned to it |
| Following activation, before269CF7 | ESP=0x1AD9F0; passed element `[ESP+4]` and retained local `[ESP+14]` both equaled0x57E79098 |
| Following return269CF9 | The same element remained at `[P+0C]` after stack cleanup |

At all captured stops the manager reported x87 CW0x007F, MXCSR0x1FA1, status0x4024 and tag0xFFFF. These are scoped native observations of those states; arbitrary volatile machine-register equality is not required or claimed. The captured actual native call269D37 directly dereferences the exact earlier input element through its retained pointer, supporting the native borrowed-input lifetime contract without synthesizing a call or changing a result/predicate.

The process continued after the last capture. The user then stopped computer control while expressly allowing background work. At the time of this report update, the manager reports a live unsaved Jan1 paused game; **normal close, debugger exit, restored-config and post-run identity/preservation checks have not been completed**. There is no shutdown/preservation claim for PID31912, and no final stable log hash. Prior PID33052's17/17 preservation and its final log remain separately valid historical evidence. Resume cleanup only after explicit computer-control authorization; background report completion does not imply that cleanup happened.
