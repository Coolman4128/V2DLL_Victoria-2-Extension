# Implementation architecture

Speed-first dependency rule (2026-10-18): follow [execution control](execution-control.md). An untouched native HPM measurement is independent of inactive fork adapters. Certify only the exact active dependency set before a modified-build call/launch; keep unsupported routes unavailable and required frozen behavior intact. Do not turn the binary-boundary module into a universal engine/renderer rewrite. Architecture discovery consumes its parent session's cumulative cap.

This is a proposed architecture, not a claim that these modules already exist. Follow the numbered sessions and verify native APIs before wiring them.

## Integration choice

Keep the upstream `lua51.dll` proxy and its retained fixes. Add focused fork modules to the same Win32 DLL, rather than basing core work on the minimal `v2dll_mod.dll` API. The latter exposes only logging, vtable replacement and a raw trampoline helper, while the features need shared lifecycle/order services and must also work in vanilla without an active mod. Preserve the optional plugin loader for mod authors.

Do not rewrite the entire 26k-line source file first. Add narrow integration seams at verified initialization, game lifecycle, UI input and safe game-update points. Only move code whose change is required by policy, performance or a new feature. Keep the existing frozen stability behavior byte-for-byte where feasible and behaviorally identical where a shared wrapper must be changed.

## Proposed files and ownership

| Area | Proposed location | Responsibility |
| --- | --- | --- |
| Patch policy/config | `Source/V2/V2/Fork/PatchPolicy.*` | Canonical settings, forbidden legacy aliases, retained fix manifest |
| Binary boundary | `Fork/EngineAdapter.*`, `Fork/AddressProfile.*` | Verified native calls, signature/ABI checks, supported binary profiles |
| Stable references | `Fork/UnitRegistry.*` | Typed IDs, campaign/owner/generation, split/merge/delete remapping |
| Command submission | `Fork/CommandBridge.*` | Normal native order constructors/queue, validation, receipts |
| Job core | `Fork/JobScheduler.*`, `Fork/Reservations.*` | Bounded human-owner jobs, reservations, cancellation, retries |
| Persistence | `Fork/JobPersistence.*`, `Fork/TemplateStore.*` | Versioned template data, save identity, atomic state, reconciliation |
| Templates | `Fork/ArmyTemplates.*`, `Fork/RecruitmentPlanner.*` | Composition snapshots, legal recruitment, gather/merge |
| Zero brigades | `Fork/ZeroStrength.*` | Reason-aware lifecycle hooks and zero-only combat exclusions |
| Transport | `Fork/TransportPlanner.*`, `Fork/TransportController.*` | Routes/fleets/capacity, prompted staged transport jobs |
| UI | `Fork/ForkUi.*`, `assets/fork/interface/`, `assets/fork/localisation/` | Native dialogs, template window, progress, fleet controls |
| Profiling | `Fork/PerfCounters.*`, `tools/benchmark/` | Cheap disabled path, timing data and result analysis |
| Validation | `tests/fork/`, `tools/verify/`, `planning/results/` | Pure-contract tests, binary verification, evidence reports |

Exact filenames may be adjusted once without changing these boundaries; record the mapping in the handoff. No new module directly owns another module's native pointers.

## Threading and authority

UI captures intent and enqueues it into the controller. At a proved safe game phase, the local human controller resolves IDs, validates resources and emits native orders. It waits for observed native state changes before advancing. Other clients receive those orders normally; they do not also run the originating player's jobs. Hotjoin/controller ownership changes suspend local jobs until identity is reconciled.

The scheduler must use game time and deterministic tie-breaking for decisions, with per-tick work/command limits. Wall-clock measurements are allowed for profiling, not to decide which simulation mutations occur. Idle controllers do no global world scans. Threads may parse local template files or compute on immutable copied data; they do not call unknown engine functions concurrently.

## Job model

Common fields: schema version, campaign/save fingerprint, job ID, controller country ID, owner generation, state, issued-command sequence, pending intent/receipt, reserved native IDs/resources, retry reason/date, creation date and cancellation generation. Persist only values/IDs, never pointers or raw engine layouts.

Template states: `Planned -> Recruiting -> Gathering -> Merging -> Complete`, with `Blocked`, `Paused`, `Canceled` transitions. A job has a template revision snapshot, target counts by type, recruit tickets and assigned brigade IDs. Reconcile actual native queue/units before reissuing anything after load.

Transport states: `Proposed -> Confirmed -> Reserving -> Rendezvous -> Splitting -> Boarding -> Sailing -> Unloading -> Returning -> Reassembling -> Complete`. Repeat the capacity-limited portion until all assigned IDs arrive. `Blocked`, `Paused`, `Canceled`, `Failed` preserve surviving native objects. Fleet/army assignments release on cancellation or invalid identity. A ferry trip completing is not job completion while other batches remain.

Both services share troop/fleet/recruitment reservations. A template job delegates a sea-crossing subjob to transport and waits; it does not independently command the same armies/fleets. Native user actions override reservations and invalidate queued automation before the next command.

Local queue invalidation does not recall an already-submitted native order. CommandBridge must prove native ordering, receipt/acknowledgment and a player-override barrier. Reconciliation must cover the interval between submission and observation, including save/load when a recruitment ticket disappears as a brigade appears. Unknown completion is a blocked reconciliation state, not permission to resubmit.

## Persistence decision gate

First prove native save IDs and lifecycle callbacks. Prefer a versioned extension block only if the save parser/writer safely preserves it and save-transfer behavior is demonstrated. Otherwise use atomic local sidecars bound to a native save/campaign identity plus explicit transfer/import/recovery. Do not corrupt `.v2` parsing by assuming unknown tokens are ignored.

The minimum guaranteed contract is safe vanilla troop loading, local job resume, no duplicate spending/units/orders and explicit handling of missing metadata. Save As, autosave rotation, compressed saves, older saves without fork data, branching from an older save, duplicate names, changes of country and different mod versions are separate cases. Never recover by nearest unit name alone.

## Native patch discipline

Each address profile carries executable fingerprint, expected bytes, complete instruction lengths, calling convention, clobbers, resume target, native preconditions and an evidence link. Existing source RVAs are research leads until validated on the local executable. A global hash mismatch must not silently produce a half-installed feature; validate its whole dependency set and install transactionally.

The current generic trampoline copies stolen bytes without relocation. New callers must prove no relative branch/call is stolen or use a validated relocating hook mechanism. The public header advertises nullable `expect`, but implementation uses `memcmp` unconditionally: never rely on that contract without fixing/validating it in S02. Avoid turning this narrow hardening into a general rewrite of old stable hooks.

## UI/mod deployment

Use engine-rendered controls rather than external keystroke automation for the product. Native modal confirmation patterns and GUI asset loading are discovered in S18/S19. Namespaced controls must coexist with vanilla and GHM's changed interfaces. Prefer additive assets/targeted integration over replacing an entire GHM window. Assets are installed with manifests/backups; an incompatible UI is detected and reported. A hidden hotkey-only prototype is not final acceptance for a requested one-click workflow.

## Performance boundaries

First remove measured observational overhead while retaining shared fixes, then validate existing optimizations, then optimize the actual hot native phase. Candidate replacements preserve side-effect order, integer/floating-point behavior and native fallback. Pure planner tests and in-game comparison runs are both needed. Do not assume that a higher FPS implies a faster year, or that more threads imply better throughput.
