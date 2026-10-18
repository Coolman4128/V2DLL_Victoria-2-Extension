# S02 static route reachability — result

- Session: `S02-20261018-static-route-reachability` (40-minute estimate; 7.919 minutes actual; S02 family wall span 1,194.124 minutes at close). Family: S02, original start `2026-10-18T00:09:43Z`.
- Bounded objective: map the two `Direct3DCreate9` IAT callsites and two statically enumerated COM slot-16 `CreateDevice` callsites, of which one was observed at runtime in the exact R02u executable; compare them with the one R02u runtime capture and determine whether static evidence shows hook-order/quiescence.
- Result: static sites and the containing routine for the first factory path are mapped. One earlier report's owner attribution is contradicted by raw function boundaries. Runtime addresses match one factory call and one CreateDevice call, but object continuity and full route coverage are not established. Hook-install ordering/quiescence remains **NOT PROVEN**. Parent S02 remains incomplete; profiles remain Rejected/WITHHELD; S03 remains gated.

## Identity and evidence chain

The exact backed-up `v2game.exe` is 12,294,656 bytes, SHA-256 `62D48C204364DD706584777C2E2B3C7AB3C5F1DD0170872554943575D53D6648`. Its backup manifest is 382,289 bytes, SHA-256 `2263245C0CEA442C33D318F0C4DEDBF1854220A068FDA612B7B944FD0AF35F6B`; the R02u `artifact-index.json` is SHA-256 `AD8AC95D7B909999D83C9582CED136B86A5657BC1B146221840EFEA17271B0DD` and anchors that manifest. `root-review/final-index-audit.json` records 152 unique anchors and zero errors. Root independently verified the raw executable and the selected code bytes.

The R02u `root-review/native-callsite-audit.json` (SHA-256 `FE9D13722D3A636988E0568EA4D50F3BE163DC8C5AB4092D17CAE627ACA1E9B3`) and `attempt02-native-callsite-audit.json` (SHA-256 `2361E8BDB610588F8AFCD1BBADB47D639E8675A89BA61E92BE13ADAD1C572BBD`) contain bounded site bytes. The accepted `sol-static/index.json` (SHA-256 `ACE0D219A21EBA2ABB3D1493F05CF5833CA693D5F0B882970398777D27EB13DA`) anchors its route artifacts; its `span-manifest.json` is SHA-256 `706491DE81F69F3528648F753625FF30913E274EC0FBB9C8DF306C7F7EA8604D`.

## Static map

`Direct3DCreate9` is the named `d3d9.dll` import at IAT RVA `0x88A39C`, preferred VA `0xC8A39C`. An independent scan of the executable `.text` section for the exact absolute-IAT call encoding found two references and no other occurrence of the IAT VA in that executable section:

| Site | RVA | Raw file offset | Bytes | Static owner/path |
| --- | --- | --- | --- | --- |
| `0x9946A2` | `0x5946A2` | `0x593AA2` | `FF 15 9C A3 C8 00` | In routine starting `0x9941E0`; returned EAX is stored to `[EBX+0x60]` at `0x9946B2`. |
| `0x9B81BC` | `0x5B81BC` | `0x5B75BC` | `FF 15 9C A3 C8 00` | In routine starting `0x9B80A0`; return EAX moves to EDI at `0x9B81C2`. The later slot-16 receiver path is not statically bound to this EDI. |

The two statically enumerated `CreateDevice` dispatches are COM virtual calls, not IAT imports or fixed targets:

| Site | RVA | Raw file offset | Bytes | Dispatch |
| --- | --- | --- | --- | --- |
| `0x9950E0` | `0x5950E0` | `0x5944E0` | `FF D2` | `call edx`; target is loaded from `[vptr+0x40]` (slot 16). |
| `0x995269` | `0x595269` | `0x594669` | `FF D0` | `call eax`; target is loaded from `[vptr+0x40]` (slot 16). |

Both dispatches are branches inside the routine starting at `0x9941E0`. In that routine, the first factory result is stored at `[EBX+0x60]`; both branches reload that field, share the output pointer `[EBX+0x64]`, and converge on a common output check. This establishes a static within-routine path for the first factory site and these two device-call alternatives, subject to the code's ordinary control flow and unchanged field value.

Raw boundary review corrects an earlier `sol-static/report.md` attribution. The report labels `0x9946A2` as belonging to a function starting at `0x994070`. The raw 363-byte function span `0x994070..0x9941DB` (SHA-256 `BBBAE0D116AD1E0B2FB96F90ECE686CAE466287ABC7E4395D5BDB7D3A3312BBF`) ends with `ret 0x8C` at `0x9941D8`. A separate conventional framed routine begins at `0x9941E0`; its raw 6,041-byte span through the `ret 0xC` at `0x995976` (range `0x9941E0..0x995979`) has SHA-256 `81EACDD039A50950A82F993E137CAED2E95BE2A3FAF55677B724B868A26645DE`. The factory call and both device dispatches lie inside this second span. The old report is preserved unchanged; its owner attribution must not be relied on.

The R02u `sol-static/report.md` also notes two additional slot-16-shaped indirect calls in the second factory routine at `0x9B8A84` and `0x9B8C9C`, whose receiver is read from `[ESP+0x60]`. Its cross-branch dataflow from the EDI factory result is unresolved. Treat them as candidate calls, not proven factory-to-device edges. This session did not extend into additional callsite enumeration.

## Runtime comparison and limits

R02u `root-review/attempt02-native-callsite-audit.json` records one original-Lua runtime factory call at `0x7C81BC` with continuation `0x7C81C2`, and one slot-16 dispatch at `0x7A5269` with continuation `0x7A526B`. Given preferred image base `0x00400000`, the consistent runtime EXE base is `0x00210000`: these addresses map to static `0x9B81BC` and `0x995269`, respectively. The capture's factory result (`0x1B9F7A0`) and CreateDevice receiver (`0x1BF5880`) are distinct; the audit explicitly declines a same-object factory-to-CreateDevice lifecycle claim. It observes those exact sites in that run only; it does not cover the other factory reference, the alternate device branch, or all candidate slot-16 dispatches.

The stock-game run did not load the fork or record a fork hook-install point. Static address order cannot prove execution order across routines, whether any route ran before a hook was installed, or that provider use was quiescent during installation. The capture stopped at EXE callsites and continuations, not provider entry; it did not establish Present/Reset callback closure, COM lifetime, teardown safety, or all-route coverage. No candidate launch, game/debugger activity, source change, build, test, deployment, or benchmark occurred in this session.

| Exit | Result |
| --- | --- |
| Exact PE and manifest identity | PASS |
| Direct factory IAT references mapped | PASS — two references in `.text` |
| Two statically enumerated slot-16 `CreateDevice` sites mapped | PASS — both virtual dispatches in one containing routine |
| Runtime address reconciliation | PARTIAL — one factory site and one device site match; object continuity and remaining routes are unproven |
| Pre-hook route order and quiescent installation phase | NOT PROVEN |
| S02 acceptance / S03 eligibility | NO — S02 incomplete and profiles withheld |
