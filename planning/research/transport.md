# Automatic transport research

Snapshot: 2026-10-17, source commit `4368f434cc33b00e7b3cc4ff2458e1f69a9ec97d`. Code inspection and local asset inspection only; no transport run has been observed in either game during planning.

## EU4 reference and evidence strength

The publisher's [Art of War feature page](https://store.steampowered.com/app/310031/Expansion__Europa_Universalis_IV_Art_of_War/) confirms automatic fleet transport and one-click army building. It does not specify all fleet selection, interruption or recombination rules.

Installed EU4 is `v1.37.5.0 Inca (491d)`, from `C:/Program Files (x86)/Steam/steamapps/common/Europa Universalis IV/launcher-settings.json`. Its `localisation/aow_l_english.yml` contains `MOVE_AUTO_TRANSPORT_DIALOG` at line 1852, a split-and-transport confirmation at 1860, a current-mission status at 1850 and a no-transport-ship explanation at 2365. The localization file SHA-256 is `55F05194F6B3305C3736F86DAEDEA361D68182B155016F65054CB8C0B052D3A4`. These are primary local evidence for the UI states, not proof of runtime path/fleet algorithms.

The Paradox wiki could not be retrieved successfully during research. A contemporary article linked the publisher's original dev diary, but that original forum link also failed to retrieve. Do not convert third-party descriptions into undocumented implementation guarantees. S19 uses controlled live EU4 cases to complete the parity ledger; the user's explicit multi-trip and recombination requirements remain binding even where secondary reports disagree.

## Existing V2 evidence

All following addresses are source/reference RVAs relative to executable base, not universally verified callable APIs. The installed executable has a different hash from the reference. See [environment.md](environment.md).

| Evidence | Location | What it establishes / does not establish |
| --- | --- | --- |
| `InstallAllyEmbark` | `V2TechButton.cpp:6146` | Three owner checks were modified for allied fleets. Disable the behavior in this fork; do not use allies for auto transport. |
| `RVA_EMBARK_OWNER_HOOK=0x1D77A0` | source:5306, reference VA `0x5D77A0` | Embark routine checks owner and later capacity/battle state. Entry signature matches local disk in a four-site sample; ABI/order flow still unproven. |
| `RVA_EMBARK_CLICK_HOOK=0x1CD058` | source:5309 | Same-province army move candidate filtering; lead for tracing an attempted move. |
| `RVA_EMBARK_CAPACITY_HOOK=0x1D5E97` | source:5329 | Order feasibility checks involving fleet ownership/capacity; do not bypass. |
| `RVA_ARMY_MOVE=0x1CCEC0` | source:16943, installer:19636 | A movement timing hook. It is not evidence that it constructs/submits an MP-safe order. |
| `RVA_FLEET_CAPTURE_HOOK=0x38F559` | source:5900 | UI-derived fleet special object and cargo list at `+0x1A4`; may help observation. Do not depend on a selected fleet UI cache for background jobs. |
| Native `FUN_005DC560` | comment:5887 | Cargo brigade count helper lead; recover ABI and meaning, including zero brigades and existing cargo. |
| Embark save repair | source:5559–5862 | Allied-owner load bug handling; tailored repair with a 40-item limit, not general persistence or a reusable arbitrary unit-ID registry. |
| Army list observation | `DeriveArmyTrueOwner`, source:5658 | Traverses army `+0x38` list and brigade POP pointer `+0x30`; lifetime/ID resolution still required. |

Already-known owner checks cannot implement an end-to-end transport mission. Required missing proofs: selected army/destination input interception, native legal path query, port/coastal routing, fleet eligibility/free capacity, native split/merge/board/unload order construction, unit identities, safe update phase and command completion/cancellation observation. Share these discoveries with template work.

## Route planning contract

Obtain the proposed destination before an invalid land move is discarded. Use the same land reachability/access test the game uses; a map-component heuristic alone is insufficient because access, occupation, straits and war can change it. If land travel is valid, retain the native order. Otherwise construct legal pickup coast, sea route, landing coast and inland leg candidates using native queries. Rank by a deterministic, documented cost matching observed EU4 selection where applicable; no straight-line distance assumption.

Plan around own eligible, non-conflicting fleets. Respect the participation toggle, existing mission/cargo and real capacity. Do not auto-merge/split escort fleets merely to simplify code. The confirmation shows which fleets will be used. Multiple eligible fleets can be assigned separate capacity reservations; the controller must also complete with a single one-slot transport.

Do not split the army or issue fleet orders until confirmation. Split as needed at a legal point, map native resulting IDs, and keep exact brigade membership in the job. Place the leader according to native split rules and preserve the desired final leader/name where allowed. A mutable leader pointer is not a persistent identifier.

Commands advance only on verified outcomes: arrival, actual embark membership, actual unload membership, merge identity. A rejected or delayed order never counts as completion. Use reasoned waiting, game-time retries and resource-change invalidation, with no per-frame path search for every idle job.

## Parity and acceptance ledger to populate in S19

| Case | Required fork result | EU4 runtime evidence required |
| --- | --- | --- |
| Legal land route | Ordinary land movement; no unwanted prompt | Observe reference input |
| Island target, enough capacity | Named-fleet prompt then automatic whole journey | Prompt, pickup and landing behavior |
| 10 brigades, 1 transport | Capacity-safe repeated trips and final 10-brigade merge | Observe exact batching/recombination; user's requirement wins if reference differs |
| 10 brigades, 3 transports | 3/3/3/1 or native-capacity-equivalent batches, one final army | Native grouping and fleet reuse |
| Two eligible fleets | Predictable choice, no double reservations | Near/far choice and opted-out fleet behavior |
| Existing cargo/mixed escort | Count free slots, leave unrelated cargo intact | Participation/selection behavior |
| Inland target on another landmass | Walk to pickup, sea transfer, inland arrival, merge at final destination | Landing choice |
| Prompt cancel/manual redirect | No side effects on cancel; manual commands prevail | Cancellation effect on returning fleet/remaining batches |
| Route invalidated mid-trip | Safe pause/replan with status; no access bypass | Native interruption behavior |
| Reload mid-board/mid-sea | No duplication/loss or stale pointer access | Reference behavior and fork persistence checks |

Record video/screenshots or a timed observation log, reference version and exact setup. No need to automate the entire EU4 interface; a small controlled reference matrix is enough. If EU4 cannot be launched, record parity as unverified and continue pure planners; final exact-parity claim stays blocked.

## Integration with zero brigades and templates

The zero-strength feature must not break capacity, splitting, army counts, native wipe detection or merge loops. Empty brigades remain assigned to their real army and convoy job. A transport fleet destroyed with embarked troops triggers the verified sea-loss path; do not silently add an exception to ZERO-1 that deletes half a stack. Define physical stack versus logical multi-trip job identity in the loss tests.

Template assembly and direct player transport use the same reservation and command services. An army cannot be simultaneously gathered by its template job and directed by a transport job; the parent waits on the child. User cancellation disconnects the dependency and preserves actual troops/paid recruitment.
