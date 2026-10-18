# R02 static VIEWS asset identity gate

The read-only `tools/verify/r02_asset_profile.py` closes the file-identity prerequisite of the narrowly observed missing-control path. It verifies fixed reviewed expectations; it never learns a mismatched installation as a new expected profile. It has no production caller and does not authorize a candidate load.

Expected identities come from existing evidence:

| Input | Pinned expectation / provenance |
| --- | --- |
| Executable | SHA-256 `62D48C204364DD706584777C2E2B3C7AB3C5F1DD0170872554943575D53D6648`, already reviewed in the native VIEWS report |
| Vanilla interface |120 files / 59 GUI; corpus fingerprint `BB56B84DFDBE52D4D45D5979B62204D18B3F9912E98A99330B15480DF0B90A36`, existing callback report |
| GHM interface overrides |7 files / 4 GUI; corpus fingerprint `B999AA8177C47E341D4B53B0E1C2956082A9975A77B0A80BE3057BF99C8DCB67`, existing callback report |
| GHM descriptor |SHA-256 `964751159215507664C8B334B5EC69A8D6E42620A8A201CD6D91AC881FE329AB`, existing callback report |
| HPM interface overrides |20 files / 6 GUI; corpus fingerprint `66A37E0A9D1DC0F36522EAF2B3C07E035FFA94E2FE2DA443F5EA13A99B26B747`, computed solely from root's retained 20-row `HPM-static-interface.json` SHA-256 `725B67B3518400325FB6DF8AE066651BAECD5B500A63119ECEAA2D02B4BDCF4E` |
| HPM descriptor |SHA-256 `D36600358E028A89497B234617AF98F106EFFD41A003A1A03A6183CBD05967B2`, official 0.4.6.3 installation evidence |

Corpus fingerprints include every regular file: ordinal POSIX relative-path order, UTF-8 concatenation of `path + TAB + lowercase SHA-256 + LF`. Missing, extra, changed or renamed files change the result. Enumeration checks each child for links before descending, then sorts the resulting rows. Linked files/directories are unsupported; Windows needs Python 3.12 or later for junction inspection and rejects older versions. All four custom lookup keys are also counted case-insensitively and must be absent. EXE mismatch rejects before asset inspection; exact mod descriptors and both vanilla/override corpora must match before `static_assets_validated` becomes true. Evidence destinations inside the repository or supplied game tree reject before inspection/writing.

Actual read-only CLI checks on the pinned local installation: **vanilla, GHM and HPM each exited 0**, with the expected counts/fingerprints, zero custom-key occurrences and matching EXE/descriptors. Reports remain outside Git at `%LOCALAPPDATA%/V2DLLFork/R02/asset-profile-contract/{vanilla,ghm,hpm}.json`. These are static file checks, not mod/game compatibility or HPM runtime acceptance.

```powershell
python tools/verify/r02_asset_profile.py --game-dir 'C:/Program Files (x86)/Steam/steamapps/common/Victoria 2' --workload hpm --output "$env:LOCALAPPDATA/V2DLLFork/R02/asset-profile-contract/hpm.json"
py -3 -m unittest tests.fork.test_r02_asset_profile -v
```

Owned temporary-fixture tests finally ran **13 tests: 12 passed, 1 skipped**. They exercise canonical nested/path/hash input independently, creation-order independence, changed/missing/extra files, case-insensitive custom keys, unknown EXE short-circuit, descriptor mismatch, missing corpus/descriptor, unknown workload, both output guards and successful file validation retaining unavailable runtime status. The actual symlink fixture was explicitly skipped because this Windows account returned WinError 1314; no executed symlink-rejection proof is claimed.

Root identified that eager `rglob` enumeration could traverse a Windows junction before checking it. The final scanner uses an explicit `os.scandir` stack and rejects each linked directory before descent. An actual owned-temp cyclic directory junction was created without elevation and correctly rejected. Cleanup used nonrecursive `os.rmdir` only on the lexically checked junction path and verified its target marker remained intact. A first test-harness cleanup attempt using PowerShell prompted under noninteractive execution; it was corrected before the final passing run. The final log SHA-256 is `EFB65C86BF3A843BAA5EACB7846381A542767BE9BD6242CBCF600FC7FF752436` (3,054 bytes). All three actual static-file profiles were checked again after the scanner change and still exited 0. Exact report/source hashes are in the private R02 artifact index.

Every success still emits `runtime_profile_status = unavailable` and `candidate_load_authorized = false`. Reads are a non-atomic file snapshot; the tool cannot prove runtime asset resolution, dynamic controls, receiver/getter subtype validity, callback phase, reentrancy, native/helper ABI or complete active startup closure. Those gates remain independent. No game, debugger, desktop, settings, native process or current product build was touched by this lane.
