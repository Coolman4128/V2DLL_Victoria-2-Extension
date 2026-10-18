# S02 second factory/device dataflow — result

- Session: `S02-20261018-second-factory-device-flow`; estimate 15 minutes, actual 3.624 minutes, family wall span approximately 1,200.16 minutes; bounded static question: do the two slot-16-shaped calls already identified in the second factory routine read the factory result?
- Input: exact R02u backup executable, 12,294,656 bytes, SHA-256 `62D48C204364DD706584777C2E2B3C7AB3C5F1DD0170872554943575D53D6648`; backup manifest SHA-256 `2263245C0CEA442C33D318F0C4DEDBF1854220A068FDA612B7B944FD0AF35F6B`; artifact index SHA-256 `AD8AC95D7B909999D83C9582CED136B86A5657BC1B146221840EFEA17271B0DD`.
- Result: **static flow supported for both candidates, conditional on ordinary stack balance/control flow**. At `0x9B81BC`, Direct3DCreate9 returns in EAX; `0x9B81C2` copies it to EDI; after `push ebx` at `0x9B81C4`, `0x9B81C5` stores EDI at `[ESP+0x64]`, corresponding to the prior frame's `[ESP+0x60]`. Each candidate later loads `[ESP+0x60]` into EAX, reads `[EAX]`, selects slot `+0x40`, and calls that target with EAX as the COM receiver.

| Candidate | Receiver setup and dispatch | Raw span | SHA-256 |
| --- | --- | --- | --- |
| A | `0x9B8A59` load, `0x9B8A5F` slot read, `0x9B8A83` push receiver, `0x9B8A84` call | file `0x5B7E59`, 45 bytes | `1E809FCFB994E0EDA6A266A3A83B831D64A84DC088BD4604CF573B269BAA9C58` |
| B | `0x9B8C71` load, `0x9B8C77` slot read, `0x9B8C9B` push receiver, `0x9B8C9C` call | file `0x5B8071`, 45 bytes | `6D5F2C219D834B4B4EEA4DD6D1F2F69B38C8E3D75C12F4250D96578C363CB06E` |

The factory sequence raw span at file `0x5B75B3`, 22 bytes, hashes to `028816C3344E22C916C94CB10BE57971DD714F3337D7CBCD3E2787BE03D0CB16`. The full containing code span `0x9B80A0..0x9B8A86` (2,534 bytes at file `0x5B74A0`) hashes to `E822914DDF7602547E022C8A6F81C55FE45A131696D424ECC4612C5FE0C81D66`. Root independently reread the raw executable, verified these hashes and decoded both 45-byte call spans with Capstone x86-32.

- Limits: this is static stack-local dataflow only. The conclusion is conditional on balanced ESP flow along the relevant paths; no runtime reachability, object identity across calls, provider route coverage, callback/lifetime, hook order or quiescent phase is proven. No candidate/game/debugger/build/test/source/deployment/benchmark activity occurred.
- S02 remains incomplete; all production profiles remain Rejected/WITHHELD; S03 remains gated; HPM speed remains NOT MEASURED. Reuse these spans rather than repeat this scan.
