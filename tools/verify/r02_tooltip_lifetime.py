"""Read-only pinned PE tooltip caller/lifetime leads. Output outside Git only.

Direct CFGs omit callees and exception edges; discovered callers are not an
exhaustive indirect caller set or lifetime/thread proof.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

import capstone
import pefile

EXPECTED = '62d48c204364dd706584777c2e2b3c7ab3c5f1dd0170872554943575d53d6648'
ENTRIES = (0x26A040, 0x2465D0, 0x5ED1D0, 0x658D40, 0x658E00,
           0x658E30, 0x5ECB60, 0x2467C0, 0x5ECA80, 0x2466D0,
           0x5A6430, 0x5A6660, 0x5A6A30, 0x5C3C70, 0x5C41D0,
           0x5EE0B0, 0x5C2F40, 0x269B60, 0x1FB7E0, 0x5A41C0,
           0x5CE470, 0x240D90, 0x657320, 0x24A6D0, 0x24C5C0,
           0x247D30, 0x5A2EC0, 0x5A3030, 0x5A4150, 0x5A4990,
           0x5A4A60, 0x248460, 0x254D80, 0x5A3000, 0x115D0,
           0x59820, 0x240B40, 0x8ED0)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--audit-calls', action='store_true',
                        help='Add bounded direct-call audit: depth3, 750 bodies, 200000 instructions')
    parser.add_argument('--audit-depth', type=int, choices=range(1, 8), default=3)
    args = parser.parse_args()
    raw = args.exe.read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    if digest != EXPECTED:
        raise SystemExit('Unrecognized executable SHA-256; no evidence generated')
    # Deliberately refuse repository-local output containing executable bytes.
    repo = Path(__file__).resolve().parents[2]
    if args.output.resolve().is_relative_to(repo):
        raise SystemExit('Executable-byte evidence output must be outside repository')
    pe = pefile.PE(data=raw)
    base = pe.OPTIONAL_HEADER.ImageBase
    decoder = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    decoder.detail = True

    def cfg(entry):
        pending, seen, unresolved = [entry], {}, []
        while pending and len(seen) < 20000:
            rva = pending.pop()
            while rva not in seen and len(seen) < 20000:
                section = pe.get_section_by_rva(rva)
                if not section or not section.Characteristics & 0x20000000:
                    unresolved.append({'rva': rva, 'why': 'nonexecutable'})
                    break
                ins = next(decoder.disasm(pe.get_data(rva, 15), base+rva, count=1), None)
                if ins is None:
                    unresolved.append({'rva': rva, 'why': 'decode'})
                    break
                seen[rva] = {'rva': rva, 'size': ins.size,
                             'bytes': ins.bytes.hex(),
                             'text': ins.mnemonic+' '+ins.op_str}
                if ins.group(capstone.CS_GRP_RET) or ins.mnemonic in ('int3', 'ud2'):
                    break
                if ins.group(capstone.CS_GRP_JUMP):
                    if ins.operands[0].type == capstone.x86.X86_OP_IMM:
                        pending.append(ins.operands[0].imm-base)
                    elif rva == 0x5A41F7:
                        # Pinned descriptor-factory switch. Reached CMP EAX,FA
                        # with unsigned JA default bounds the first index0..250;
                        # byte-map indices0..15 select relocated target words.
                        indices = set(pe.get_data(0x5A4508, 251))
                        if max(indices) > 15:
                            raise ValueError('Unexpected pinned factory switch')
                        for index in indices:
                            target = struct.unpack('<I', pe.get_data(0x5A44C8+index*4, 4))[0]-base
                            pending.append(target)
                    elif rva == 0x2072CD:
                        # The pinned Budget switch has a reached unsigned
                        # EAX<=6 guard; include all seven HIGHLOW table targets.
                        for index in range(7):
                            pending.append(struct.unpack('<I', pe.get_data(0x20B070+index*4, 4))[0]-base)
                    else:
                        unresolved.append({'rva': rva, 'why': 'indirect jump'})
                    if ins.mnemonic == 'jmp':
                        break
                rva += ins.size
        return {'entry': entry, 'instructions': sorted(seen.values(), key=lambda x: x['rva']),
                'unresolved': unresolved, 'exhausted': len(seen) >= 20000}

    result = {'sha256': digest, 'capstone': capstone.__version__,
              'pefile': pefile.__version__, 'cfg': [cfg(r) for r in ENTRIES],
              'caller_candidates': []}
    def word(rva):
        return struct.unpack('<I', pe.get_data(rva, 4))[0]
    table = 0xA0A34C
    locator = word(table-4)-base
    descriptor = word(locator+12)-base
    result['idler_table'] = {
        'rva': table, 'locator': locator,
        'rtti': pe.get_data(descriptor+8, 100).split(b'\0')[0].decode(),
        'hover_entry': word(table+0x4C)-base,
        'tooltip_entry': word(table+0x198)-base,
        'frame_entry': word(table+4)-base,
    }
    result['descriptor_name_targets'] = [
        {'table': table, 'target': word(table+0x18)-base}
        for table in (0xA41D20, 0xA3CACC, 0xA41EE0)
    ]
    result['native_retained_element_slots'] = [
        {'table': table, 'slot': table+0x9C, 'target': word(table+0x9C)-base}
        for table in (0xA38738, 0xA3A930)
    ]
    # Scan all possible E8/E9 positions first; accept a candidate boundary only
    # if reached by forward CFG from the recorded prologue lead. No claim that
    # every possible entry point has a conventional frame or direct call.
    for section in pe.sections:
        if not section.Characteristics & 0x20000000:
            continue
        data = section.get_data()
        for offset in range(max(0, len(data)-4)):
            if data[offset] not in (0xE8, 0xE9):
                continue
            rva = section.VirtualAddress+offset
            target = rva+5+struct.unpack('<i', data[offset+1:offset+5])[0]
            if target != 0x26A040:
                continue
            start = max(0, rva-0x4000)
            prefix = pe.get_data(start, rva-start)
            prologue = prefix.rfind(bytes.fromhex('55 8b ec'))
            if prologue < 0:
                result['caller_candidates'].append({'rva': rva, 'call_reached': False})
                continue
            parent = cfg(start+prologue)
            reached = any(i['rva'] == rva and i['size'] == 5 and
                          i['text'].split()[0] in ('call', 'jmp')
                          for i in parent['instructions'])
            result['caller_candidates'].append({'rva': rva, 'entry': start+prologue,
                                                'call_reached': reached, 'cfg': parent})
    if args.audit_calls:
        imports = {i.address: d.dll.decode()+'!'+(i.name.decode() if i.name else str(i.ordinal))
                   for d in pe.DIRECTORY_ENTRY_IMPORT for i in d.imports}
        pending = [(r, 0, [r]) for r in (0x206780, 0x3010A0, 0x2D2DD0)]
        bodies, frontier, instruction_count = {}, [], 0
        while pending:
            entry, depth, path = pending.pop(0)
            if entry in bodies:
                continue
            if len(bodies) >= 750 or instruction_count >= 200000:
                frontier.append({'entry': entry, 'why': 'global audit budget', 'path': path})
                continue
            section = pe.get_section_by_rva(entry)
            if not section or not section.Characteristics & 0x20000000:
                frontier.append({'entry': entry, 'why': 'nonexecutable call target', 'path': path})
                continue
            body = cfg(entry)
            instruction_count += len(body['instructions'])
            calls, indirect, import_calls = [], [], []
            for index, ins in enumerate(body['instructions']):
                text = ins['text']
                if not text.startswith(('call ', 'jmp dword ptr [0x')):
                    continue
                if text.startswith('call 0x'):
                    target = int(text.split()[1], 16)-base
                    calls.append({'site': ins['rva'], 'target': target})
                    if depth < args.audit_depth:
                        pending.append((target, depth+1, path+[target]))
                    elif target not in bodies:
                        frontier.append({'entry': target, 'why': 'depth limit', 'path': path+[target]})
                elif text.startswith(('call dword ptr [0x', 'jmp dword ptr [0x')):
                    address = int(text.split('[0x')[1].split(']')[0], 16)
                    if address in imports:
                        import_calls.append({'site': ins['rva'], 'import': imports[address]})
                    else:
                        indirect.append({'site': ins['rva'], 'text': text,
                                         'context': body['instructions'][max(0, index-5):index+1]})
                elif text.startswith('call '):
                    indirect.append({'site': ins['rva'], 'text': text,
                                     'context': body['instructions'][max(0, index-5):index+1]})
            bodies[entry] = {'entry': entry, 'depth': depth, 'path': path,
                             'instruction_count': len(body['instructions']),
                             'cfg_unresolved': body['unresolved'], 'cfg_exhausted': body['exhausted'],
                             'direct_calls': calls, 'indirect_calls': indirect, 'imports': import_calls}
        result['call_audit'] = {'depth_limit': args.audit_depth, 'body_limit': 750,
                                'instruction_limit': 200000, 'instructions': instruction_count,
                                'bodies': list(bodies.values()), 'frontier': frontier}
        targets = {item['entry'] for item in frontier}
        result['call_audit']['frontier_distinct_targets'] = len(targets)
        result['call_audit']['frontier_unvisited_distinct_targets'] = len(targets-set(bodies))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps({'output': str(args.output),
                      'sha256': hashlib.sha256(args.output.read_bytes()).hexdigest(),
                      'direct_candidates': len(result['caller_candidates'])}))


if __name__ == '__main__':
    main()
