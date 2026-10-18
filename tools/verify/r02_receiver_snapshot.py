"""Run with GDB `source` while the original game is stopped.

Read-only snapshot of already resolved receiver pointers in convenience variables:
$r02_host (Technology selected_tech_window), $r02_budget_container and
$r02_factory_host (Production factory_buttons). No native function is invoked.
Layout evidence: planning/results/R02-native-views.md, native FindChild 0x5E4C80
and its bounded two-list search helper 0x5E6B30. Raw results stay outside Git.
This observes one stopped state; it does not certify future lifetime/asset closure.
"""
import hashlib
import json
import os
import re
from pathlib import Path

import gdb

EXPECTED = "62d48c204364dd706584777c2e2b3c7ab3c5f1dd0170872554943575d53d6648"
executable = Path(gdb.current_progspace().filename)
if hashlib.sha256(executable.read_bytes()).hexdigest() != EXPECTED:
    raise RuntimeError("Unsupported on-disk executable; no receiver snapshot")
inferior = gdb.selected_inferior()
if not inferior.pid or any(thread.is_running() for thread in inferior.threads()):
    raise RuntimeError("The owned native process must be stopped")
match = re.search(r"^\s*(0x[0-9a-fA-F]+)\s*-\s*0x[0-9a-fA-F]+ is \.text\s*$",
                  gdb.execute("info files", to_string=True), re.MULTILINE)
if not match:
    raise RuntimeError("Cannot establish mapped main-image base")
base = int(match.group(1), 16) - 0x1000


def read(address, length):
    if address <= 0 or length < 0 or length > 65536 or address + length > 0x100000000:
        raise RuntimeError("Invalid or oversized PE32 observation")
    return bytes(inferior.read_memory(address, length))


def u32(address):
    return int.from_bytes(read(address, 4), "little")


def string(address):
    size, capacity = u32(address + 0x10), u32(address + 0x14)
    if size > 4096 or capacity < size:
        raise RuntimeError("Invalid native name string")
    storage = address if capacity <= 15 else u32(address)
    raw = read(storage, size + 1)
    if raw[-1] != 0:
        raise RuntimeError("Native name string lacks terminator")
    return raw[:-1].decode("ascii")


if read(base, 2) != b"MZ":
    raise RuntimeError("Mapped main-image identity header unavailable")
rows = []
for label, variable, keys in (
    ("Technology selected_tech_window", "$r02_host", ["FE_ACADEMIES_BDSM", "FE_RPROJECTS_BDSM"]),
    ("Budget container", "$r02_budget_container", ["FE_BUDGET_DIPLO_BDSM"]),
    ("Production factory_buttons", "$r02_factory_host", ["hide_colonial_states"]),
):
    receiver = int(gdb.parse_and_eval(variable))
    vptr = u32(receiver)
    if vptr != base + 0xA3C0D8 or u32(vptr + 0x34) != base + 0x5E4C80:
        raise RuntimeError("Unexpected FindChild receiver subtype/target: " + label)
    fallback_vptr, root = u32(receiver + 0x370), u32(receiver + 0x374)
    if fallback_vptr != base + 0xA41E84:
        raise RuntimeError("Unexpected embedded fallback type: " + label)
    names = []
    lists = []
    for name_offset, object_offset in ((0x27C, 0x26C), (0x29C, 0x28C)):
        begin, end = u32(receiver + name_offset), u32(receiver + name_offset + 4)
        objects, object_end = u32(receiver + object_offset), u32(receiver + object_offset + 4)
        if end < begin or (end - begin) % 28 or (end - begin) // 28 > 512:
            raise RuntimeError("Invalid bounded native name-vector extent")
        count = (end - begin) // 28
        if object_end < objects or object_end - objects != count * 4:
            raise RuntimeError("Native object/name-vector counts disagree")
        entries = []
        for index in range(count):
            name = string(begin + index * 28)
            entries.append({"name": name, "object": u32(objects + index * 4)})
            names.append(name.lower())
        lists.append({"name_offset": name_offset, "count": count, "entries": entries})
    found = {key: key.lower() in names for key in keys}
    rows.append({"label": label, "receiver": receiver, "vptr_rva": vptr - base,
                 "fallback_vptr_rva": fallback_vptr - base, "fallback_root": root,
                 "lists": lists, "requested_keys_found": found,
                 "missing_key_null_inferred": root == 0 and not any(found.values()),
                 "inference_basis": "observed list absence and zero root; independently decoded FindChild CFG; no missing-key call executed"})
result = {"pid": inferior.pid, "executable_sha256": EXPECTED, "mapped_base": base,
          "scope": "stopped native state; no engine calls/writes; no future lifetime certification",
          "receivers": rows}
output = Path(os.environ["LOCALAPPDATA"]) / "V2DLLFork/R02/native-receivers" / f"pid-{inferior.pid}.json"
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(result, indent=2), encoding="utf-8")
print("R02 receiver snapshot:", output)
for row in rows:
    print(row["label"], "list counts", [item["count"] for item in row["lists"]],
          "fallback root", hex(row["fallback_root"]),
          "requested keys found", row["requested_keys_found"],
          "missing-key null inferred from static CFG", row["missing_key_null_inferred"])
