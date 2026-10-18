# S02 — original-game D3D route and installation-phase disposition

Session: `S02-20261018-route-phase` (45-minute estimate). This is a bounded reuse of the accepted R02u/R02v evidence; no new game, debugger, build, source change, hook, or native observation was run.

## Route evidence

| Stage | Runtime observation | Static inventory | What it establishes |
| --- | --- | --- | --- |
| Factory acquisition | EXE callsite `0x007C81BC`, continuation `0x007C81C2`, Thread 1/TID 27928; returned factory `0x01B9F7A0`; SDK argument `0x20`; factory vtable `0x6F8D2C88` in authenticated pinned d3d9 image | Pinned EXE has factory IAT callsites at preferred VAs `0x009946A2` and `0x009B81BC` | One factory call pair was observed. The second static callsite and any paths not in this inventory were not demonstrated absent or covered. |
| Device creation | EXE callsite `0x007A5269`, continuation `0x007A526B`, same Thread 1/TID 27928; receiver `0x01BF5880` differs from factory returned above; `S_OK`; device `0x0D4DE080`; device vtable `0x0D4E0FDC` is writable `MEM_PRIVATE` | Pinned EXE has indirect CreateDevice sites at preferred VAs `0x009950E0` and `0x00995269` | One CreateDevice pair and one private heap vtable were observed. The factory-to-device object lifecycle is not continuous in the evidence; native provider construction before output publication is only a bounded static path in R02v. |
| Provider methods | Observed factory CreateDevice slot 16 target `d3d9+0x40050`; device Reset/Present targets `d3d9+0xE06A0` / `d3d9+0xE02A0`; provider identity and mapped `.text` normalization match pinned d3d9 | R02v traces bounded A/B template copying, Reset recopy and Release/delete paths | This identifies the sampled provider route and explains the sampled object layout. It does not establish all drivers, templates, device variants, or method invocations. |

Raw evidence reviewed: private R02u immutable index SHA-256 `AD8AC95D7B909999D83C9582CED136B86A5657BC1B146221840EFEA17271B0DD` and R02v index SHA-256 `8FAB8A1331635C787994F9455ED1C59A3DE38CEB65E75B9BAF62910691971E90`, both matching their recorded hashes. Root's R02u native callsite audit records exactly the two observed pairs, one PID, TID 27928, and the limitations above. The read-only region records tie the EXE callsites and provider target to the expected image regions. The static callsite audit lists two factory IAT sites and two CreateDevice indirect sites; static presence is not runtime coverage.

## Installation phase disposition

**BLOCKED / NOT PROVEN.** R02u observed one ordinary original-Lua/HPM factory/CreateDevice path with hardware breakpoints and read-only memory queries. It did not load the fork, invoke its installer, or observe an installation opportunity. The capture does not prove that every device route is seen before the first device is created, that all relevant creation occurs on one exclusive thread/phase, or that the phase is quiescent against concurrent D3D use. R02v adds a static provider producer path before output publication, but it cannot establish the game's runtime publication ordering or exclusive phase. A CreateDevice-only hook profile is also incompatible with retaining current HookPresent behaviors and frozen FPU defaults, per the preceding profile-scope result.

No safe/read-only native experiment can answer the missing install-phase question using the current captures alone. The smallest next native access would require a separately scoped, backed-up original-game run that observes all identified factory/CreateDevice callsites and lifecycle ordering across the relevant startup/menu path, with no fork candidate or writes. That can improve route coverage but still cannot prove quiescence at a future fork install point unless the candidate's actual install timing is safely observable. Until that timing is evidenced, every production D3D profile remains Rejected/WITHHELD; do not launch or relax existing guards.

## Session exit

- Exact observed routes and static site inventory: **RECORDED**, with coverage limits.
- Exclusive original-game installation phase: **BLOCKED / NOT RUN**; no inference from the SDK fixture or static provider trace.
- Build, tests, launch, deployment, and benchmark: **NOT RUN**.
- S02 and S03: **S02 INCOMPLETE; S03 GATED**. HPM speed remains **NOT MEASURED** because P00 was skipped by user direction.
- Timing lesson: the short record was fast because it reused the indexed callsite audit and region records. This estimates evidence synthesis only; it does not imply native phase work is similarly short. No task is added by the review.
