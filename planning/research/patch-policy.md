# Patch policy and vanilla restoration research

Research snapshot: 2026-10-17, commit `4368f434cc33b00e7b3cc4ff2458e1f69a9ec97d`, source version `5.83`. This is a source audit, not an in-game validation or a claim that the bundled DLL was built from this commit. Implementation has not started.

All source references below use `Source/V2/V2/V2TechButton.cpp` unless another path is given. Line numbers are navigation hints for this snapshot; search the symbol after edits. Main evidence: `Settings` (50), `EXE_PATCHES` (4167), `ApplySetting` (4429), `WriteDefaultSettings` (4628), `LoadSettings` (4955), `Install` (25003), and `V2DLL_LATEST_INSTALL/v2dll_settings.ini`.

User clarification: the selected mod and primary benchmark workload is **Historical Project Mod (HPM), corrected from the earlier GHM identification**. Vanilla is the original-engine behavior baseline; HPM is the main compatibility and performance workload. When running HPM, preserve its intended content plus retained defect corrections and the three requested features. Do not reset HPM's defines, events, technology, units, economy data or other intentional mod rules to stock Victoria 2 values. HPM compatibility still requires inspecting the exact supplied release; this audit covers V2DLL source, not an assumed HPM data set.

## Decision rules

- Keep actual defect corrections: wrong instruction/register use, invalid pointer protection, wrong modifier indexing, and displays that disagree with existing simulation values. A name containing `FIX` is not sufficient evidence.
- Remove unrelated V2DLL rule/balance changes from the fork's effective installation. Change compiled defaults, generated defaults, shipped settings, aliases, and install gating together. A zero default alone is insufficient: reject/ignore attempts to enable removed features through legacy keys, generic byte-patch aliases, or mod-local overrides and apply a final policy guard before installation. Keeping unreachable upstream implementation for mergeability is acceptable; deleting every old function is unnecessary.
- Preserve scripting capabilities where they are inert without explicit mod content. Gameplay-changing scripted examples must not be activated by the base package. Default minting and building purchases off; retain parsers/extension code for explicitly opted-in mods.
- Keep the existing multiplayer stabilization implementation and effective simulation settings frozen, including its state-savings cap and POP quantization. These alter numeric state, but are explicitly retained bug-fix exceptions to vanilla behavior. Diagnostic observation may be disabled or made cheaper by the performance workstream if shared simulation fixes remain identical; logging flags are not frozen simulation settings. Multiplayer is **DONE — awaiting user testing**. No new multiplayer stabilization sessions.
- The three user-requested army features are explicit exceptions to original behavior. Their implementation may use existing multiplayer command paths, but must not introduce a general networking redesign.
- Ambiguous bug-fix classifications are **provisional until S04's evidence gate**. Reproduce the original defect, trace the changed instruction/data/rule, and compare the correction with intended native behavior and HPM compatibility. Retain demonstrated defect corrections, including economic/AI fixes; remove an option only when the evidence identifies an added policy/balance rule rather than a correction. If uncertainty remains, keep the item explicitly pending and use native behavior for the candidate baseline; do not mark the “retain all bug fixes” requirement complete. Do not quietly keep V2DLL balance changes because the upstream author calls them improvements.

## Critical findings that prevent an INI-only cleanup

1. **Two advertised options do not work.** `ApplySetting` lines 4467–4484 nests the `ALWAYS_ADD_WARGOALS` and `FACTORY_ALLOW_UNCIV_CONSTRUCTION` comparisons inside the `FACTORY_ALLOW_CONSTRUCTION_IN_COLONIAL` block. A key cannot match both strings, so those branches are unreachable. Their byte patches default on. Fix the braces and test all three options independently. Existing generic `PATCH_ALWAYS_ADD_WARGOALS` and the four `PATCH_BUILD_FACTORY_*UNCIVILIZED*` aliases are usable but are not a satisfactory permanent workaround.
2. **A hidden gameplay change defaults on:** `war_on_puppet_allowed` at RVA `0x53AB18` (4366) allows war against subjects/puppets. It is absent from the distributed and generated INI. Set its compiled and explicit profile default off; also keep its two tooltip companions off.
3. **Generated keys disagree with the parser.** `WriteDefaultSettings` emits `REMOVE_SLEEPERS` and `BUILDING_GOODS_CONSUMPTION`; `ApplySetting` has neither. Actual accepted controls are legacy `PATCH_MP_CLIENT_SLEEP` / `PATCH_MAIN_LOOP_SLEEP0` and `ENABLE_GOODS_CONSUMPTION`. Thus changing the emitted keys currently does nothing. Define canonical keys, support existing aliases, emit each canonical key once, and reject/report unknown keys. Do not alter effective multiplayer sleep settings while fixing parsing.
4. **Hidden true default:** `PATCH_NEEDS_HONEST_UI` is parsed (4491), defaults true (107), and is installed, but is missing from the shipped/generated INI. Retain and expose this UI correction.
5. **Not all disabled options prevent installation.** `PATCH_CIVILIZE_NULL_CHECK` is forcibly set true at 25009; `InstallDestroyCmdNullCheck()` has no toggle; `InstallGoodsConsumption()` always installs parser hooks even when purchasing is disabled. Those behaviors are intentional crash/parser corrections and should be documented, not accidentally stripped.
6. **Do not trust friendly names or stale comments.** `trade_max_price_20x_multiplier` currently has bytes described by the implementation as an experimental 40x constant. `navyAllowUseAllyTransports` has a comment saying disabled by default but is true. A `FACTORY_CLOSE_DRY_RUN` string exists in dead logging text; no active dry-run setting exists.
7. **Defaults are layered, not replaced.** `LoadSettings` reads root settings then, only if root `LOCAL_MOD_CONFIG=1`, applies mod settings over the existing state. Missing keys inherit prior values. Generic `PATCH_<byte-patch-name>` aliases can override grouped options later in a file. Define deliberate precedence and log effective values; do not assume a partial mod INI resets defaults.

## Complete settings disposition

The tables enumerate the distributed settings, additional accepted settings, and all byte-patch aliases. `1/0` are recommended target fork settings, not current values. Entries marked “S04 provisional” require the evidence decision before final policy enforcement. For confirmed removed gameplay changes, `0` means **enforced disabled**, including aliases and mod overrides; optional cosmetic/scripting settings remain user configurable. “Frozen” means preserve the current effective simulation value/implementation pending user testing. “Performance” means retain the option and evaluate only as part of the performance workstream, under behavioral equivalence checks.

### Military and unrelated gameplay rules

| Setting | Target | Reason / implementation evidence |
| --- | --- | --- |
| `ALWAYS_ADD_WARGOALS` | 0 | Enables debug always-add-wargoal behavior; broken parser branch must be repaired; byte patch `always_add_wargoals`. |
| `ARMY_FIX_BAD_REINFORCEMENT` | 1 | Removes carrying reduced supply forward into the next brigade; byte patch at file offset `0x1C809B`. |
| `NAVY_FIX_BAD_REINFORCEMENT` | 1 | Corrects register transfer direction (`89` to `8B`), file offset `0x1C7F1C`. |
| `ARMY_BETTER_REINFORCE_ON_ALLIED_OWNED_LAND` | 0 | Raises allied reinforcement from 100% to 150%, plus `InstallOccupiedReinforceSplit` / `InstallAllyOwnerCheck`; disable all three together. |
| `NAVY_ALLOW_USE_ALLY_TRANSPORTS` | 0 | Unrequested alliance rule extension. Gates embark hooks, load ownership repair, passenger tooltip, and capture hook in `Install`. New auto transport must use own eligible fleets. |
| `COMBAT_ENABLE_ROLL_CHANGES` | 0 | Custom rolls; installer depends on a pre-existing external roll-changer code cave. |
| `COMBAT_ROLL_MIN`, `COMBAT_ROLL_MAX` | Inert when off | Preserve parsing for legacy profiles; do not imply that writing vanilla min/max repairs an already modified executable. |
| `COMBAT_SECOND_LINE_SUPPORT_UNIT_PRIORITY` | 0 | Entire replacement reserve deployment behavior (`InstallReserveFrontline`). |
| `COMBAT_SECOND_LINE_DAMAGE_REDIRECT` | 0 | 80/20 frontline/backline damage split (`InstallSecondLineSplash`). |
| `COMBAT_LOSSES_VISIBLE_TO_EVERYONE` | 0 | UI-only but exposes information to nonparticipants; original information availability is the conservative default. |
| `PATCH_REMOVE_CONSCIOUSNESS_PLURALITY_GROWTH` | 0 | Removes normal monthly plurality growth. |
| `PATCH_ALLOW_UNCIV_TECH_RESEARCH` | 0 | Changes research eligibility; already off and questionable signature. |
| `PATCH_WAR_ON_PUPPET_ALLOWED` | 0 | Hidden true-default subject-war rule change. |
| `PATCH_WAR_ON_PUPPET_TOOLTIP_1`, `PATCH_WAR_ON_PUPPET_TOOLTIP_2` | 0 | Tooltip companions; already off. |

### Economic behavior and economic bug fixes

| Setting | Target | Reason / implementation evidence |
| --- | --- | --- |
| `TRADE_PRICE_DELTA_PERCENTUAL` | 0 | Replaces original price step. |
| `TRADE_PRICE_DELTA_EXPONENTIAL` | 0 | Alternative replacement at same site, takes precedence over percentage mode; already off. |
| `TRADE_MAX_PRICE_20X_MULTIPLIER` | 0 | Price ceiling constant experiment; name is misleading. |
| `FACTORY_ALLOW_CONSTRUCTION_IN_COLONIAL` | 0 | Disable three byte patches plus `InstallProdListVisibilityHook` / `InstallProdTypeGateHook`. |
| `FACTORY_COLONIAL_ALLOW_LOCAL_SUPPLY` | 0 | Independent colonial eligibility bypass; turning off the preceding setting alone does not disable this. |
| `FACTORY_ALLOW_UNCIV_CONSTRUCTION` | 0 | Disable four byte patches; repair unreachable parser branch. |
| `FACTORY_COLONIAL_EXTRA_WHITELIST` | Empty | Currently `fishery`; inert with colonial gates off. |
| `FACTORY_COLONIAL_ALLOW_ALL` | 0 | Accepted but omitted from generated/shipped INI; broadens colonial factory whitelist. |
| `FACTORY_REFUND_SAVINGS_ON_CLOSE` | 0 recommended; S04 provisional | `InstallFactoryClose` adds payouts on multiple closure routes. Cash conservation alone does not establish intended transfer behavior. Trace ownership of saved funds and native closure payout to distinguish a lost-money defect from a new distribution rule; retain a proven correction. Keep crash protection separate. |
| `FACTORY_AUTO_CLOSE_UNPROFITABLE` | 0 | Adds a configurable closure timer and finance hook. |
| `FACTORY_AI_SPAM_FIX` | 0 recommended; S04 provisional | `InstallAiExpandStaffing` adds a minimum staffing requirement to AI expansion. Determine whether this restores a missing/broken native eligibility condition or introduces a new AI policy. Retain demonstrated defect correction; a preference for smarter expansion alone does not establish a bug. |
| `FACTORY_FORCE_MIN_WAGE` | 0 | Adds fixed wages independent of profit. |
| `PATCH_AI_NAVAL_BASE_LIMIT` | 0 | Adds new per-state AI construction limit; already off. |
| `FACTORY_AI_FACTORY_PRIORITY_RULESWAP` | 1 recommended; verify in S04 | `InstallPriorityByRule` (21763) corrects checking `delete_factory_if_no_input` instead of `factory_priority`; source records aligned vanilla rules give identical outcomes. Verify this on stock rules and HPM's combinations, and retain the demonstrated rule-wiring correction. Do not conflate this patch with the separate factory staffing policy. |
| `POP_CARD_NEEDS_AFFECTED_BY_NEEDS_MODIFIERS_FIX` | 1 | `InstallPopCardStrataMods`: UI shows modifiers already used by simulation. |
| `POP_BUDGET_PAYOUT_NEEDS_SCALED` | 0 recommended; S04 provisional | `InstallBudgetPayoutByNeeds` (22054) changes actual wages/transfers and leaves freed money in treasury; intended vanilla treatment not established. Prove whether the native budget contract requires scaling before classification; retain any demonstrated correction. Distinct from the established modifier-indexing defect. |
| `POP_FIX_NEEDS_MODIFIERS` | 1 | `InstallFixStrataNeedsMods` (22110 vicinity): middle/rich life/everyday descriptors are ordered incorrectly relative to the slots consumers use. |
| `PATCH_NEEDS_HONEST_UI` | 1 | `InstallNeedsHonestUi`: reflect actual common money-pool coverage in display; hidden default must be exposed. |
| `ENABLE_MINTING` | 0 | `InstallMinting` adds national revenue when formula present. Keep extension code; no active minting formula in vanilla package. |
| `BUILDING_GOODS_CONSUMPTION` | 0, canonicalize | Emitted but unparsed. Repair alias to `ENABLE_GOODS_CONSUMPTION`; default spending off while retaining parser support. |
| `ENABLE_GOODS_CONSUMPTION` | 0, legacy alias | Actual parsed setting. `InstallGoodsConsumption` (10607) reads script blocks regardless, returns before market purchases when false. |

### Crash fixes, UI, configuration, and script extensions

| Setting | Target | Reason / implementation evidence |
| --- | --- | --- |
| `LOCAL_MOD_CONFIG` | 0 | Preserve root-first behavior; allow deliberate mod overrides with effective-value reporting. |
| `PATCH_CIVILIZE_NULL_CHECK` | 1 | Forced crash guard; retain even with uncivilized factory feature removed. |
| `PATCH_SUPPLY_SOURCE_NULL_CHECK` | 1 | Crash correction. |
| `PATCH_TECH_NULL_CHECK_FIXES` | 1 | Both technology comparison and folder-icon null guards. |
| `PATCH_GRAPH_POINT_CLAMP` | 0 | Already off; leave optional UI guard dormant until a reproduced defect requires it. |
| `FIX_BUDGET_TOTAL_INCOME_LABEL` | 1 | Two byte patches remove subtraction of tariffs already included by the tooltip. |
| `ENABLE_BUTTONS` | 0 for legacy built-ins | Current `BUTTONS` (521) hardcodes three mod-specific academy/research/exchange decisions. Retain generic mechanism for new requested UI; do not ship those actions enabled by default. |
| `ENABLE_DECISION_FILTER` | 0 for legacy built-ins | Hides the same hardcoded mod decisions; old content should not be hidden unexpectedly. New template/transport UI receives separate gating. |
| `ENABLE_VERSION_LABEL` | 1 | Helpful fork identity, cosmetic. |
| `HIDE_UNAVAILABLE_LIMIT_BY_SUPPLY_FACTORIES` | 0 | Mod-specific list filtering; conservative original UI. |
| `HIDE_RAW_GOODS_PRODUCTION_FILTER_BUTTONS` | 0 | Hides `raw_` goods; specific to upstream mod content. |
| `FILTER_SHOW_ALL_FACTORIES_IN_STATE` | 0 | Changes meaning of factory filtering. |
| `FACTORY_FILTER_BY_PRODUCERS_ONLY` | 0 | Removes input consumers from search results; original UI default. |
| `ENABLE_MUSIC_PLAYER` | 1 | Cosmetic, custom button absent in stock GUI is expected; no gameplay changes. |
| `MUSIC_FAIR_RANDOM` | 1 | Fixes track random selection based on uninitialized stack memory. Verify it remains separate from simulation RNG. |
| `GOODS_ICONS_AS_SEPARATE_FILES` | 1 | Script-driven asset extension; no replacement when no icon override provided. |
| `ENABLE_RESTART_GAME_BUTTON` | 1 | Convenience UI; retain with active-mod behavior. |
| `ENABLE_ASSIMILATE_BLOCK` | 1 | Retain block-form script support in `V2DLL_AssimilateBlock.cpp`; no unsolicited events. |
| `ENABLE_EVENT_SOUNDS` | 0 | Existing off default; retain optional `EventMusic.cpp` plugin. |
| `LOAD_DLL_FROM_MOD_FOLDER` | 1 | Retain active mod `v2dll_mod.dll` extension interface. No extra gameplay plugin in base package. |

### Existing stability, performance, and diagnostics

| Setting | Disposition |
| --- | --- |
| `PATCH_FPU_FORTRESS=1` | Frozen stability. `PinFpu()` is also called unconditionally; an off flag does not make the process wholly vanilla. |
| `PATCH_D3D_FPU_PRESERVE=1` | Frozen stability. |
| `PATCH_THREAD_FPU_PIN=1` | Frozen stability. |
| `PATCH_POP_QUANTIZE=1`, `POP_QUANTIZE_KEEP_BITS=12` | Frozen stability; intentionally rounds fixed-point POP fields. |
| `PATCH_STATE_SAVINGS_CAP=1`, `STATE_SAVINGS_CAP_POUNDS=60000` | Frozen stability; transfers excess state savings to country treasury. Do not remove as an economy rebalance. |
| `ENGINE_WORKER_THREADS=0` | Preserve baseline. Source means “leave TBB count alone,” not single-threaded. Any future performance experiment must preserve frozen stability behavior. |
| `PATCH_HEAP_LFH=1` | Retain existing memory optimization. |
| `REMOVE_SLEEPERS=1` | Fix parser without changing effective baseline. |
| `PATCH_MP_CLIENT_SLEEP`, `PATCH_MAIN_LOOP_SLEEP0` | Accepted aliases both write `removeSleepers`, not independent booleans; canonicalization must define conflict precedence. |
| `MP_CLIENT_SLEEP_MS=1`, `MAIN_LOOP_SLEEP_MS=1` | Accepted but omitted in generated/distributed INI; retain baseline values. |
| `PATCH_D3D_NO_VSYNC=0`, `D3D_FPS_LIMIT=70` | Performance/presentation settings; benchmark with actual renderer and documented FPS cap. |
| `FIX_SFX_MIXER_LAG=1`, `FIX_ARMY_WINDOW_LAG=1` | Retain lag fixes; actual army-window path is nested-idle handling, not the obsolete standalone installer. |
| `PATCH_HIGH_PRIORITY=1` | Existing priority/power policy; evaluate effect in benchmarks. |
| `PATCH_COMPRESS_FLAG_CACHE=1` | Existing experimental DXT1 asset cache; retain option, verify startup and visual impact. |
| `PATCH_PREFETCH_POPS=1` | Existing prefetch optimization; retain and measure. |
| `PATCH_CULL_DECISION_CHECKS=1` | Existing behavior-equivalent filtering claim; validate invalidation. |
| `PATCH_CULL_CB_CHECKS=1` | Existing behavior-equivalent filtering claim; validate invalidation. |
| `PATCH_PREFETCH_RELATIONS=1`, `PATCH_PREFETCH_AI_RELATIONS=1` | Retain and measure. |
| `PATCH_FAST_AI_QUEUE=1` | Retain existing queue optimization for baseline; depends on frame/speed behavior. |
| `PATCH_PARALLEL_DECISIONS=1`, `PATCH_PARALLEL_EVENTS=1` | Preserve existing baseline; modes 0/1/2 mean off/use workers/shadow compare. Mode 2 is a performance-correctness investigation tool, not a new multiplayer improvement project. |
| `ENABLE_LOG=1`, `DEBUG_LOG=0` | Preserve diagnostic capability; benchmark release vs instrumented profiles explicitly. |
| `ENABLE_OOS_LOG=1`, `ENABLE_RNG_PROBE=1`, `ENABLE_STATE_PROBE=1` | Record these as upstream baseline values, not mandatory release logging levels. Preserve diagnostic capability and frozen multiplayer simulation. Observation can be disabled or made cheaper for performance; disabling a trace must not disable the savings-cap fix or any other simulation work. |
| `ENABLE_CRASH_LOG=1`, `ENABLE_CRASH_DUMP=0` | Preserve crash reporting defaults. Handlers install before settings are loaded; emitted report behavior is gated later. |
| `SP_WINDOW_FROM`, `SP_WINDOW_TO`, `MONEY_WATCH_TAGS` | Preserve empty defaults and existing diagnostics; source says empty money-watch tags defaults to `KUK`, not no watched country. |

## Every `EXE_PATCHES` record

Each record also accepts case-insensitive `PATCH_<record_name>` via the fallback parser. These are additional public controls, even if not written to the INI. Group aliases must not leave individual records enabled. `F` is PE file offset; `R` is RVA. Do not treat them interchangeably.

| Record | Location | Current compiled / target |
| --- | --- | --- |
| `always_add_wargoals` | F `0x137EFF` | 1 / 0 |
| `army_fix_bad_reinforcement` | F `0x1C809B` | 1 / 1 |
| `navy_fix_bad_reinforcement` | F `0x1C7F1C` | 1 / 1 |
| `trade_max_price_20x_multiplier` | R `0xA45C28` | 1 / 0 |
| `remove_consciousness_plurality_growth` | R `0x10C5DE` | 1 / 0 |
| `army_better_reinforce_on_allied_owned_land` | R `0x1D74BE` | 1 / 0 |
| `combat_losses_visible_to_everyone` | R `0x19CC5D` | 1 / 0 |
| `budget_total_income_tariffs_1` | R `0x2010CC` | 1 / 1 |
| `budget_total_income_tariffs_2` | R `0x2010D6` | 1 / 1 |
| `build_factory_ignore_colonial_1` | R `0xD0C57` | 1 / 0 |
| `build_factory_ignore_colonial_2` | R `0x12FA4E` | 1 / 0 |
| `build_factory_button_enable_ignore_colonial` | R `0x12E977` | 1 / 0 |
| `factory_colonial_allow_local_supply` | R `0xD0E9D` | 1 / 0 |
| `build_factory_ignore_uncivilized_button` | R `0x12E96E` | 1 / 0 |
| `build_factory_checklist_uncivilized_own` | R `0x12EA67` | 1 / 0 |
| `build_factory_checklist_uncivilized_other` | R `0x12F0D4` | 1 / 0 |
| `build_factory_ignore_uncivilized_can_build` | R `0x12CA3E` | 1 / 0 |
| `allow_unciv_tech_research` | F `0x3A9B57` | 0 / 0 |
| `war_on_puppet_allowed` | R `0x53AB18` | 1 / 0 |
| `war_on_puppet_tooltip_1` | R `0x53B07A` | 0 / 0 |
| `war_on_puppet_tooltip_2` | R `0x53B10F` | 0 / 0 |

## Always-installed / indirect paths requiring explicit review

- `Install` patches all `VIEWS` slots even with `ENABLE_BUTTONS=0`; `OnViewUpdate` uses separate flags for features. Audit shared callbacks when introducing the new army UI.
- `InstallFactoryDumpScan` (7600) creates a background scanner thread without a setting. Its heap scans are not disabled merely by `ENABLE_LOG=0`. Profile this in the performance workstream; first prove whether the data feeds a required retained feature before gating/removing it.
- `InstallArmySelectDiag` (19622) installs timing hooks unconditionally. Old disabled experimental branches (`if(false)`) and comment-only `PATCH_SKIP_SEL_PROJ` are not functioning settings.
- `InstallEngineStability` (20020) unconditionally calls `PinFpu`, timer resolution setup and late-module patching. `DllMain` (26329) calls `InstallCrashWatch` and `PinFpu` before loading settings. Freeze existing stabilization; do not use an “all settings off” run as a vanilla control.
- `InstallChecksumDiagnostic`, `InstallLobbyEntryHook`, `InstallOosWatch`, `InstallRngProbe`, `InstallEventRollProbe`, `InstallStateProbe` and the clipboard worker are invoked by `Install`; inspect internal gating rather than assuming invocation means a setting is honored. Preserve multiplayer paths.
- `InstallDestroyCmdNullCheck` is an unconditional retained crash fix; disabling factory economics must not remove it.
- `InstallGoodsConsumption` always installs the script property parser; setting false only prevents purchasing hooks. This matches retaining scripting while removing base economic behavior.
- `EnsureV2dllDefines` can migrate legacy `minting.txt` into `defines_v2dll.txt` and rename the old file. Its callers include factory features and minting. Avoid invoking legacy migration solely to enumerate defaults during tests.
- `LoadProductionTypeLimits` (3167) and configuration fingerprinting (24970 vicinity) prefer hardcoded `mod\\2\\common\\production_types.txt`; province-history helpers also deserve active-mod routing review. Use the active mod or stock data consistently; do not accidentally inherit this upstream mod's content.
- `V2DLL_LATEST_INSTALL` contains binary DLLs and `dgVoodoo.conf`, not a full stock-data comparison. Record exact game executable, DLC/patch, renderer/wrapper, mod files, and loaded plugins before claiming vanilla behavior. An externally patched roll-change executable will remain modified after this fork disables its hook.

## Concrete implementation packets (serial integration, each under two hours)

These are recommendations for the root session plan, not additional sessions to run independently of it.

1. **Config repair and explicit registry: 90–110 minutes.** One owner edits `V2TechButton.cpp` parser/defaults; parallel independent agent prepares a settings test harness and checks emitted keys. Repair unreachable branches and aliases; enumerate hidden byte patches and hidden defaults; report unknown/conflicting keys; preserve effective frozen values. Deliver root/mod/absent-file/alias precedence tests and parser-to-writer coverage.
2. **Vanilla policy implementation: 90–110 minutes.** One owner applies target defaults and enforced install guards after S04 resolves provisional classifications; a parallel agent owns docs/distribution settings and independent policy review. Retain demonstrated bug fixes and script parsing; no active unsolicited upstream minting/purchase rules; disable all 21-record table changes marked off. Test hostile/stale legacy aliases and mod-local attempts to re-enable removed changes. Preserve HPM's own intended data rules. Merge without concurrent edits to the large source file. Deliver effective manifest showing every installed patch and why it is enabled. If S04 needs additional reproduction time, keep its unresolved decision pending and split that work into another bounded packet; never silently substitute a final removal for missing evidence.
3. **Compatibility validation: 90–120 minutes.** Run targeted HPM scenarios, with stock controls where useful: permitted/blocked factory construction, wages/transfers, price steps/caps, war goals/subject wars, plurality, combat rolls/deployment/damage, needs UI and modifier mapping, reinforcement, and retained crash guards. Compare an unpatched engine running the same HPM release and the fork under identical seed/save/config where practical; document expected differences from retained fixes and new features. A parallel reviewer traces missing-key and mod-override paths and reviews binary signatures. If a runnable game is absent, produce the exact fixture/test checklist and mark runtime gate pending rather than claiming completion.

Acceptance is not “all settings in the distributed INI are zero.” It is that all nonrequested gameplay hooks and byte patches are absent in the effective installed manifest, compiled/generated/distributed settings agree, real fixes remain active, scripting is inert without opt-in content, and the frozen multiplayer stabilization behavior is preserved.
