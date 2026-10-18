"""Read-only narrow S02 frozen-boundary evidence; raw results stay outside Git.

Requires pefile and capstone. This is not a complete disassembler/ABI verifier:
direct CFG traversal does not resolve indirect branches or prove runtime phase.
"""
import argparse
import hashlib
import json
from pathlib import Path

import capstone
import pefile
from capstone.x86 import X86_OP_IMM


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    raw = args.exe.read_bytes()
    pe = pefile.PE(data=raw)
    image = pe.get_memory_mapped_image()
    base = pe.OPTIONAL_HEADER.ImageBase
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True

    def instruction(ins):
        return {"rva": hex(ins.address - base), "bytes": ins.bytes.hex(" "),
                "size": ins.size, "mnemonic": ins.mnemonic, "operands": ins.op_str}

    def disasm(rva, size):
        return [instruction(ins) for ins in md.disasm(image[rva:rva + size], base + rva)]

    def cfg(entry, limit=30000):
        pending = [base + entry]
        seen, returns, unresolved, outside = {}, [], [], []
        while pending and len(seen) < limit:
            address = pending.pop()
            while address not in seen:
                rva = address - base
                if not 0 <= rva < len(image) or abs(rva - entry) > 0x10000:
                    outside.append(hex(rva))
                    break
                ins = next(md.disasm(image[rva:rva + 15], address, count=1), None)
                if ins is None:
                    unresolved.append({"rva": hex(rva), "reason": "decode failure"})
                    break
                seen[address] = instruction(ins)
                if ins.group(capstone.CS_GRP_RET):
                    returns.append(instruction(ins))
                    break
                if ins.group(capstone.CS_GRP_JUMP):
                    if ins.operands and ins.operands[0].type == X86_OP_IMM:
                        pending.append(ins.operands[0].imm)
                    else:
                        unresolved.append(instruction(ins))
                    if ins.mnemonic == "jmp":
                        break
                if ins.mnemonic in ("int3", "ud2"):
                    break
                address += ins.size
        return {"instructions": [seen[k] for k in sorted(seen)], "returns": returns,
                "unresolved": unresolved, "outside": outside,
                "budget_exhausted": bool(pending and len(seen) >= limit)}

    relocs = [entry.rva for block in getattr(pe, "DIRECTORY_ENTRY_BASERELOC", [])
              for entry in block.entries if entry.type != 0]
    sites = {}
    for name, rva, expected, steal in [
        ("pop", 0x85E40, "55 8b ec 64 a1 00 00 00 00", 9),
        ("daily_savings", 0x282EC0, "55 8b ec 6a ff", 5),
        ("mainloop", 0x5DF550, "55 8b ec 6a ff", 5),
    ]:
        signature = bytes.fromhex(expected)
        decoded = list(md.disasm(image[rva:rva + steal], base + rva))
        sites[name] = {
            "rva": hex(rva), "source_signature": expected,
            "signature_match": image[rva:rva + len(signature)] == signature,
            "stolen": [instruction(ins) for ins in decoded],
            "complete_stolen_decode": sum(ins.size for ins in decoded) == steal,
            "stolen_relative_control_flow": [instruction(ins) for ins in decoded
                if ins.group(capstone.CS_GRP_JUMP) or ins.group(capstone.CS_GRP_CALL)],
            "stolen_relocations": [hex(x) for x in relocs if rva <= x < rva + steal],
            "resume_rva": hex(rva + steal), "entry_and_resume": disasm(rva, 100),
            "cfg": cfg(rva),
        }
    post = bytes.fromhex("81 ec 40 01 00 00 53 56 8b 75 08")
    sites["daily_savings"]["source_post_seh_match"] = image[0x282ED8:0x282ED8 + len(post)] == post
    report = {
        "exe": str(args.exe), "sha256": hashlib.sha256(raw).hexdigest(),
        "md5": hashlib.md5(raw).hexdigest(), "machine": hex(pe.FILE_HEADER.Machine),
        "image_base": hex(base), "image_size": hex(pe.OPTIONAL_HEADER.SizeOfImage),
        "capstone": capstone.__version__, "pefile": pefile.__version__,
        "sites": sites,
        "caller_windows": {"pop_8468f": disasm(0x8466B, 64),
                           "daily_32ecfa": disasm(0x32ECF0, 16),
                           "main_57e0c8": disasm(0x57E0A4, 60)},
        "limits": "Read-only static direct CFG; no indirect branch resolution, runtime/thread/lifetime proof, or OOS investigation.",
    }
    report["caller_boundary_checks"] = {}
    for name, start, call in [("pop", 0x84060, 0x8468F),
                              ("daily", 0x32ECF0, 0x32ECFA),
                              ("mainloop", 0x5796B0, 0x57E0C8)]:
        traversal = cfg(start)
        report["caller_boundary_checks"][name] = {
            "entry_rva_from_reference_metadata": hex(start),
            "call_rva": hex(call),
            "call_reachable_by_direct_cfg": any(int(i["rva"], 16) == call
                for i in traversal["instructions"]),
            "instruction_count": len(traversal["instructions"]),
            "unresolved": traversal["unresolved"],
            "outside": traversal["outside"],
            "budget_exhausted": traversal["budget_exhausted"],
        }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps({"sha256": report["sha256"], "sites": {
        k: {x: v[x] for x in ("signature_match", "complete_stolen_decode", "stolen_relocations")}
        for k, v in sites.items()}}, indent=2))


if __name__ == "__main__":
    main()
