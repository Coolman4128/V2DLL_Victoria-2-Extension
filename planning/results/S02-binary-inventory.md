# S02 binary inventory lane

Status: **read-only evidence delivered; complete active profile is not certified**. No executable, installed DLL/configuration, live process or game UI was changed by this lane. Shared source belongs to the integrator. Inventory was generated on 2026-10-17 against source hashes recorded in its private JSON; rerun after further shared edits.

## Identity and method

Local `v2game.exe`: MD5 `63C5485FF00353F2BE8DAAA3F0DCF957`, SHA-256 `62D48C204364DD706584777C2E2B3C7AB3C5F1DD0170872554943575D53D6648`, x86 PE32, preferred base `0x400000`. Research-reference MD5 is `1D1A97E42BD2DD65DEABD3E7F707B27D`. The reference executable is not available, so this is a comparison against independent source expectations and existing research, not a complete binary-to-binary diff.

`tools/verify/binary_inventory.py` uses PE section mapping and Capstone package 5.0.9 (its Python module reports 5.0.7), with pefile 2024.8.26. Dependencies were installed only under `%LOCALAPPDATA%/V2DLLFork/S02/python`. Source comment stripping preserves line numbers. Brace-scoped bindings prevent another function's same-named constant/signature from supplying a false match; arrays modified after initialization are unresolved. Every relied-upon literal binding was reviewed in the emitted owner/definition/use listing, including local `SIG`, `sigRebuild`, `sigThis`, `sigOvl` and other repeated names. No observed executable bytes were copied into an expected signature.

Run from the repository in PowerShell:

```powershell
$env:PYTHONPATH = "$env:LOCALAPPDATA/V2DLLFork/S02/python"
python tools/verify/binary_inventory.py --exe 'C:/Program Files (x86)/Steam/steamapps/common/Victoria 2/v2game.exe' --source-root Source/V2/V2 --output "$env:LOCALAPPDATA/V2DLLFork/S02/binary-inventory.json"
```

Private raw bytes/disassembly and machine-readable evidence remain outside Git in that S02 directory. `binary-inventory.json` contains source identities, section mapping, all extracted call/access records, expected-signature declaration lines, and unresolved records. `binary-inventory-summary.json` contains the report identity and mismatch detail. `roll-sites.json` contains the separately traced roll sites. No proprietary binary is distributed.

## Findings

- All **21 EXE_PATCHES** compare equal to their source original expectations; none equal their replacement bytes. This does not enable them or endorse their gameplay changes. Configuration can still activate unwanted table records before S03 policy enforcement.
- **143 literal comparisons**, covering **129 distinct source/RVA/length spans**: **141 match, 2 fail**. Counts include repeated preflight/install checks and dormant code; they are not counts of installed patches. All 143 have an accessible recorded expected declaration. Two mismatches are the CombatRoll cave checks below.
- **41 resolved StealToTrampoline calls** have no relative control instruction in their stolen spans and end at a locally decoded instruction boundary. All resolved non-memcmp hook spans end at local decoded boundaries. This proves neither a trusted function entry nor absence of incoming control flow into a stolen range. No new native ABI is certified by linear decoding.
- **31 WriteJmpSite invocations** have literal expected lengths equal to their requested overwrite lengths in this source snapshot; all extracted resume comparisons match. Some resume checks intentionally cover only an opcode prefix and do not certify the full destination instruction or all downstream branches.
- Three dormant POP-display table entries match their six-byte expectations. Source search finds no live InstallPopSite invocation. Four unconditional VIEWS vtable slots are mapped, but source supplies no original-target expectations; these remain unsupported pending a justified profile.
- **378 lexical call records** and **722 address/access leads** retain unresolved/dynamic/helper/data records explicitly. These counts include declarations and non-engine memcmp/IAT operations. They are an exhaustive ledger for the tool's documented lexical patterns, not a compiler-derived exhaustive active dependency graph.

## Combat-roll and prepatched gameplay evidence

The source expects an externally created Roll Changer cave at RVA `0x889113`. The prefix comparison at `0x889115` and middle comparison at `0x889122` fail: the required added `mov ecx, immediate` and division/add block are absent. The existing matching E9 fragment belongs to preceding native code and is not evidence that the external gameplay patch is present.

Starting disassembly from native function RVA `0x19CA40`, all four research-identified roll sites at RVAs `0x19CA97`, `0x19CAAD`, `0x19CB1B`, `0x19CB31` contain **cdq; mov ecx, 10; idiv ecx**, rather than a call to the external cave. Research's committed `z_The Great Library/v2_battle_re/decomp/FUN_0059ca40_0059ca40.c` instead shows `FUN_00c8911d()` at the corresponding locations. This specifically rules out that described four-site Roll Changer redirection on the local executable. It does not prove there are no other external modifications anywhere in the file. `InstallCombatRoll` correctly cannot apply to this local cave; S03 must disable the unrelated option by policy independently.

## Unresolved contracts that block whole-profile certification

1. Vtable slots: `PatchSlot` checks neither an expected native target nor full image range. Its API/helper declarations and dynamic object-vtable replacements cannot be certified from a static slot observation. The four VIEWS slots are unconditionally installed. Other static slots include decision validity, fleet tooltip, budget update, building property parsing and assimilation execution. Some callers check their own original target; those checks must become explicit profile dependencies rather than treating the generic helper as validated.
2. Runtime-derived stolen bytes: RNG C/D have independently checked opcode plus rebased counter operands, but need the full compound check represented in the profile. PhysFS close constructs a rebased absolute operand. `sigPump` copies its own eight bytes and checks only four; province-dirty uses its own bytes after checking a six-byte head for a ten-byte steal; brigade-tail copies six bytes but checks only two. Those self comparisons do not establish independent signatures. Both active and literal-false copies appear in the ledger.
3. Computed E8 relationships and optimization tables: PopPrefetch, DecisionCull, RelationPrefetch, CbCull, FastAiQueue and ParallelEvents need their complete target, call-site, data/vtable and fallback dependencies bound to their own source namespaces. Self-passed `(base + site)` expected pointers are not independent byte evidence. Multi-declaration constants and fixed base-relative uses are recorded, but array-indexed targets and complex expressions remain unresolved.
4. Compound direct installers: byte tables, flag-cache IAT CALL replacements, needs/modifier swap tables, reserve-frontline auxiliary branches, event-roll sites, sleep immediate modifications, topbar/frame call replacements, dynamic D3D/IAT hooks and optional mod API users need explicit install-order/rollback/activation review. The ledger retains every VirtualProtect occurrence (including allocated caves/dynamic APIs), rather than pretending only generic helper calls perform writes.
5. Safety beyond equality: the integrator added fail-closed validation for null expected bytes and invalid helper arguments/image ranges, plus an HDE32 guard rejecting relative, malformed or truncated stolen instructions before writes. Raw copies still do not relocate relative instructions. Legacy protection restoration, output-pointer publication and multi-site partial-install failure require transactional testing. Assimilation publishes a vtable change before validating its effect-factory hook. RNG/event multi-site installs can leave a partial subset. `WriteJmpSite` needs argument bounds and cache maintenance even though current literal callers' complete overwrite lengths match.
6. Calling conventions, register/FPU preservation, native object lifetime, verified thread/safe phase and dynamic runtime-vtable validity still require their own source/control-flow/runtime evidence. Synthetic tests and menu launch cannot certify these. No native command adapter is authorized by this inventory.

The next contract is a reviewed per-feature dependency graph with independently justified expected bytes/targets and exact overwritten/resume spans, followed by transactional installation checks. The integrator must explicitly block every unsupported retained feature before claiming a complete active profile. R02 is applicable if that proof cannot finish inside S02; partial matching cannot close the gate.

## Tool validation and limits

A private extraction fixture passed: a valid scoped literal was compared successfully; a same-name use in another function remained unresolved; a runtime-mutated byte array remained unresolved. This is a test of report conservatism only. Read-only inspection and the fixture did not change executable identity. `git diff --check` passed for the new tool. After the integrator added the narrow HDE guard, the final inventory was refreshed without signature changes. All 41 resolved raw-trampoline literal spans were exported to private `raw-trampoline-fixtures.json` for testing the actual helper. Prior evidence is preserved as `binary-inventory-snapshot01.json` and its summary. No native hook was installed and no live debugger/process/UI was used.

The parser is intentionally lexical: macros, preprocessor alternatives, arbitrary expressions, dataflow, callgraph reachability and activation are not fully interpreted. An instruction decoded from an arbitrary start can be plausible yet not the real engine boundary. Therefore “match” below means only scoped source expectation equals local file bytes. Source owner and literal-false status make dormant matches visible; all other activation remains conditional/unresolved until configuration and callgraph review.

## Literal comparison ledger

The following table is generated from the private snapshot. `use/def` are source line numbers; all entries are `V2TechButton.cpp` except the explicitly prefixed assimilation row. `false` means inside a literal `if(false)` block, `no caller` means source search found no live installer caller, and `review` means conditional activation remains to be established. It is not an installed manifest.

| Owner | use/def | RVA | bytes | result | activation |
| --- | --- | --- | ---: | --- | --- |
| V2DLL_AssimilateBlock.cpp: InstallAssimilateBlock | 744/742 | `0x476B30` | 6 | match | review |
| InstallApplyVolumesHook | 1645/1535 | `0x35BB50` | 7 | match | review |
| InstallMenuCloseObserverHook | 1863/1860 | `0x266814` | 10 | match | review |
| InstallMusicFairRandom | 2138/2088 | `0x5534D` | 6 | match | review |
| InstallProdListVisibilityHook | 2209/2188 | `0x2F424B` | 2 | match | review |
| InstallGoodsFilterProbe | 2372/2312 | `0x2E28BF` | 5 | match | review |
| InstallProdTypeGateHook | 3523/3387 | `0xD04BC` | 10 | match | review |
| InstallHideNoSupplyFactoriesHook | 3790/3561 | `0x2F9E41` | 3 | match | review |
| InstallPriceDelta | 3988/3982 | `0x82BA9` | 5 | match | review |
| InstallExponentialPriceDelta | 4091/3982 | `0x82BA9` | 5 | match | review |
| InstallOccupiedReinforceSplit | 5081/5074 | `0x1D751B` | 5 | match | review |
| InstallEmbarkLoadRepair | 5833/5617 | `0x1D7CC4` | 8 | match | review |
| InstallFleetCaptureHook | 6021/5904 | `0x38F559` | 6 | match | review |
| InstallAllyEmbark | 6153/5354 | `0x1D77A0` | 12 | match | review |
| InstallAllyEmbark | 6158/5357 | `0x1CD058` | 14 | match | review |
| InstallAllyEmbark | 6163/5364 | `0x1D5E97` | 14 | match | review |
| InstallCivilizeNullCheck | 6515/6508 | `0x14248B` | 7 | match | review |
| InstallSupplySourceNullCheck | 6696/6689 | `0xD15EB` | 6 | match | review |
| InstallTechCompareNullCheck | 6803/6793 | `0x3A918A` | 12 | match | review |
| InstallTechFolderIconNullCheck | 6941/6930 | `0x3ADE98` | 15 | match | review |
| InstallGraphPointClamp | 7121/7110 | `0x5E0FD6` | 13 | match | review |
| InstallVersionLabel | 7773/7758 | `0x233826` | 8 | match | review |
| InstallBudgetWindowHooks | 9180/9178 | `0x20A0B8` | 6 | match | review |
| InstallMinting | 9219/9215 | `0x12B516` | 13 | match | review |
| InstallMinting | 9220/9217 | `0x12B523` | 3 | match | review |
| InstallMinting | 9245/9242 | `0x12B769` | 6 | match | review |
| InstallMinting | 9272/9264 | `0x12A135` | 6 | match | review |
| InstallMinting | 9272/9266 | `0x12B0A3` | 6 | match | review |
| InstallMinting | 9274/9264 | `0x12A135` | 6 | match | review |
| InstallMinting | 9278/9266 | `0x12B0A3` | 6 | match | review |
| InstallGoodsConsumption | 10646/10643 | `0x8466B` | 8 | match | review |
| InstallGoodsConsumption | 10659/10655 | `0x12B5E1` | 18 | match | review |
| InstallGoodsConsumption | 10666/10663 | `0x12B354` | 5 | match | review |
| InstallGoodsConsumption | 10673/10670 | `0x30E859` | 6 | match | review |
| InstallGoodsConsumption | 10680/10677 | `0x30F3A4` | 10 | match | review |
| InstallGoodsConsumption | 10690/10687 | `0x79C07` | 7 | match | review |
| InstallCombatRoll | 10753/10737 | `0x889115` | 9 | **MISMATCH** | review |
| InstallCombatRoll | 10754/10739 | `0x889122` | 4 | **MISMATCH** | review |
| InstallReserveFrontline | 11006/10813 | `0x19B190` | 13 | match | review |
| InstallSecondLineSplash | 11226/11072 | `0x19A8E0` | 6 | match | review |
| InstallSecondLineSplash | 11227/11070 | `0x19AC8A` | 9 | match | review |
| InstallChecksumDiagnostic | 11361/11301 | `0x238A40` | 11 | match | review |
| InstallLobbyEntryHook | 11451/11397 | `0x36B4F6` | 6 | match | review |
| InstallOosWatch | 12176/11501 | `0x282ED8` | 11 | match | review |
| InstallMainLoopFpuPin | 13800/12226 | `0x5DF550` | 5 | match | review |
| InstallRngProbe | 15109/15100 | `0x5B7950` | 6 | match | review |
| InstallRngProbe | 15109/15100 | `0x5B7AC0` | 6 | match | review |
| InstallRngProbe | 15124/15100 | `0x5B7950` | 6 | match | review |
| InstallRngProbe | 15125/15100 | `0x5B7AC0` | 6 | match | review |
| InstallEventRollProbe | 16170/15909 | `0x4A7EB1` | 6 | match | review |
| InstallEventRollProbe | 16171/15910 | `0x4A8092` | 6 | match | review |
| InstallWindowFps | 19543/19539 | `0x240680` | 5 | match | review |
| InstallWindowFps | 19546/19540 | `0x240B90` | 8 | match | review |
| InstallWindowFps | 19549/19541 | `0x41A450` | 6 | match | review |
| InstallWindowFps | 19554/19553 | `0x254D80` | 6 | match | review |
| InstallWindowFps | 19559/19558 | `0x257B60` | 5 | match | review |
| InstallWindowFps | 19562/19558 | `0x2592F0` | 5 | match | review |
| InstallWindowFps | 19567/19566 | `0x5AE320` | 6 | match | review |
| InstallWindowFps | 19572/19570 | `0x5EB7C0` | 9 | match | review |
| InstallWindowFps | 19575/19558 | `0x3F7CE0` | 5 | match | review |
| InstallWindowFps | 19578/19566 | `0x59C370` | 6 | match | review |
| InstallWindowFps | 19584/19582 | `0x254620` | 9 | match | review |
| InstallWindowFps | 19587/19582 | `0x248460` | 9 | match | review |
| InstallWindowFps | 19590/19558 | `0x1F7A50` | 5 | match | review |
| InstallWindowFps | 19593/19558 | `0x24F350` | 5 | match | review |
| InstallWindowFps | 19598/19596 | `0x55290` | 9 | match | review |
| InstallWindowFps | 19602/19601 | `0x588F20` | 5 | match | review |
| InstallWindowFps | 19607/19605 | `0x254530` | 7 | match | review |
| InstallWindowFps | 19625/19624 | `0x2859C0` | 5 | match | review |
| InstallArmySelectDiag | 19651/19648 | `0x3FC360` | 6 | match | review |
| InstallArmySelectDiag | 19657/19649 | `0x1CC530` | 6 | match | review |
| InstallArmySelectDiag | 19660/19649 | `0x1CCEC0` | 6 | match | review |
| InstallArmySelectDiag | 19663/19649 | `0x26A7F0` | 6 | match | review |
| InstallArmySelectDiag | 19666/19649 | `0x1D4540` | 6 | match | review |
| InstallArmySelectDiag | 19681/19679 | `0x1CC5B0` | 6 | match | false |
| InstallArmySelectDiag | 19681/19680 | `0x1CCA3B` | 2 | match | false |
| InstallArmySelectDiag | 19707/19706 | `0x398A50` | 5 | match | false |
| InstallArmySelectDiag | 19713/19712 | `0x26A958` | 6 | match | false |
| InstallArmySelectDiag | 19744/19742 | `0x26AE98` | 10 | match | false |
| InstallArmySelectDiag | 19769/19768 | `0x393510` | 5 | match | false |
| InstallArmySelectDiag | 19775/19773 | `0x393290` | 9 | match | false |
| InstallArmySelectDiag | 19783/19781 | `0x39332D` | 9 | match | false |
| InstallArmySelectDiag | 19789/19788 | `0x391BB0` | 5 | match | false |
| InstallArmySelectDiag | 19795/19794 | `0x3810A0` | 6 | match | false |
| InstallArmySelectDiag | 19813/19811 | `0x393570` | 5 | match | false |
| InstallArmySelectDiag | 19823/19820 | `0x393290` | 9 | match | review |
| InstallArmySelectDiag | 19829/19827 | `0x391BB0` | 5 | match | review |
| InstallArmySelectDiag | 19835/19833 | `0x3810A0` | 6 | match | review |
| InstallArmySelectDiag | 19865/19859 | `0x391BEE` | 5 | match | review |
| InstallArmySelectDiag | 19867/19860 | `0x391BFE` | 6 | match | review |
| InstallArmySelectDiag | 19869/19861 | `0x391C2D` | 9 | match | review |
| InstallArmySelectDiag | 19871/19863 | `0x391C44` | 5 | match | review |
| InstallArmySelectDiag | 19879/19878 | `0x5B2750` | 6 | match | review |
| InstallArmySelectDiag | 19884/19882 | `0x5B275C` | 5 | match | review |
| InstallArmySelectDiag | 19894/19893 | `0x38AF3A` | 5 | match | review |
| InstallArmySelectDiag | 19903/19902 | `0x38AF44` | 5 | match | review |
| InstallArmySelectDiag | 19912/19911 | `0x38AF49` | 5 | match | review |
| InstallArmySelectDiag | 19923/19920 | `0x38B152` | 9 | match | review |
| InstallArmySelectDiag | 19930/19929 | `0x38B183` | 5 | match | review |
| InstallArmySelectDiag | 19954/19943 | `0x5E4507` | 6 | match | review |
| InstallArmySelectDiag | 19956/19944 | `0x5E4595` | 6 | match | review |
| InstallArmySelectDiag | 19959/19945 | `0x5E4672` | 6 | match | review |
| InstallArmySelectDiag | 19961/19946 | `0x5E46A5` | 6 | match | review |
| InstallArmySelectDiag | 19963/19947 | `0x5E46D5` | 6 | match | review |
| InstallArmySelectDiag | 19965/19948 | `0x5E4705` | 6 | match | review |
| InstallArmySelectDiag | 19967/19949 | `0x5E4735` | 6 | match | review |
| InstallArmySelectDiag | 19969/19950 | `0x5E4762` | 9 | match | review |
| InstallArmySelectDiag | 19985/19982 | `0x5E46C9` | 7 | match | review |
| InstallArmySelectDiag | 19993/19992 | `0x38B1C2` | 5 | match | review |
| InstallArmySelectDiag | 20001/20000 | `0x38B1CD` | 5 | match | review |
| InstallWaitDiagHooks | 20036/20033 | `0x2481D0` | 5 | match | review |
| InstallWaitDiagHooks | 20038/20034 | `0x2B70A0` | 6 | match | review |
| InstallWaitDiagHooks | 20040/20035 | `0x254D80` | 6 | match | review |
| InstallPopQuantize | 20270/20112 | `0x85E40` | 9 | match | review |
| InstallFactoryClose | 20883/20880 | `0xF5750` | 6 | match | review |
| InstallFactoryClose | 20890/20887 | `0xF50C0` | 10 | match | review |
| InstallFactoryClose | 20897/20894 | `0x834C6` | 6 | match | review |
| InstallFactoryClose | 20907/20904 | `0xF4B30` | 6 | match | review |
| InstallAiExpandStaffing | 21005/21001 | `0x457614` | 7 | match | review |
| InstallFactoryMinWage | 21368/21365 | `0xF4F6F` | 8 | match | review |
| InstallFactoryMinWage | 21375/21372 | `0xF4C0E` | 8 | match | review |
| InstallDestroyCmdNullCheck | 21423/21422 | `0x17D052` | 4 | match | review |
| InstallDestroyCmdNullCheck | 21430/21420 | `0x17D01F` | 6 | match | review |
| InstallPriorityByRule | 21792/21789 | `0x8155B` | 5 | match | review |
| InstallPopCardStrataMods | 21927/21913 | `0x583E16` | 5 | match | review |
| InstallPopCardStrataMods | 21927/21914 | `0x583F11` | 5 | match | review |
| InstallPopCardStrataMods | 21927/21915 | `0x584025` | 5 | match | review |
| InstallPopCardStrataMods | 21932/21913 | `0x583E16` | 5 | match | review |
| InstallPopCardStrataMods | 21934/21914 | `0x583F11` | 5 | match | review |
| InstallPopCardStrataMods | 21936/21915 | `0x584025` | 5 | match | review |
| InstallBudgetPayoutByNeeds | 22087/22080 | `0x10B9B0` | 9 | match | review |
| InstallBudgetPayoutByNeeds | 22088/22082 | `0x10BDD9` | 6 | match | review |
| InstallBudgetPayoutByNeeds | 22094/22082 | `0x10BDD9` | 6 | match | review |
| InstallBudgetPayoutByNeeds | 22097/22080 | `0x10B9B0` | 9 | match | review |
| InstallNeedsHonestUi | 22183/22169 | `0x85960` | 6 | match | review |
| InstallNeedsHonestUi | 22186/22171 | `0x85390` | 6 | match | review |
| InstallNeedsHonestUi | 22197/22173 | `0x55CEF8` | 12 | match | review |
| InstallNeedsHonestUi | 22199/22174 | `0x55D488` | 12 | match | review |
| InstallNeedsHonestUi | 22201/22175 | `0x55DA18` | 12 | match | review |
| InstallNeedsHonestUi | 22203/22177 | `0x5820B8` | 6 | match | review |
| InstallNeedsHonestUi | 22207/22205 | `0x3BB105` | 6 | match | review |
| InstallPhysfsOpenReadHook | 23283/23282 | `0x721F40` | 6 | match | review |
| InstallGoodsIcons | 23314/23312 | `0x7206E0` | 7 | match | review |

## Snapshot identities

Private final inventory SHA-256: `30DE2F65527EF6E16D85DAA818995EF96C63AF331C5976CAFA599986D01050CD`.
Private raw-trampoline fixtures SHA-256: `9CE1839579AEAFF08016D70AE466997CB956258428510286E40B48849E2D2591`.

| Source | SHA-256 |
| --- | --- |
| V2DLL_AssimilateBlock.cpp | `05643C14E75224A915F497EAD13E4FCD52F5EC4FEB4E61F3099AB5C0346077EC` |
| V2DLL_ModLoader.cpp | `0A2E75A6ED741DA411DA05745FF9A8A860B453C424F41FE18D1AB61BA9D2179A` |
| V2DLL_Shared.h | `5FC80E22FBABAD05A70D9F69448C068D543BBCE4B9968A3925C8DAE80473D346` |
| V2TechButton.cpp | `4164E90F773E2A15C1045FBEE71E4B0EF224C41F7F8E8953E7BFBC1B8DFB9EBE` |

These identify the observed final source snapshot. Rerun the tool after integration if those files change.
