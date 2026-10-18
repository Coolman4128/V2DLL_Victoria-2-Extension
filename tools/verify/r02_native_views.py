"""Read-only VIEWS PE/RTTI/CFG evidence; output belongs outside Git.

Requires pefile and capstone. Static reachability does not certify runtime ABI,
thread, object lifetime or every indirect caller.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

import capstone
import pefile

VIEWS = [('CTechnologyView', 0xA17FA4, 11), ('CBudgetView', 0xA059F0, 10),
         ('CProductionView', 0xA0FECC, 10), ('CPoliticsView', 0xA0E458, 10)]
EXPECTED = '62d48c204364dd706584777c2e2b3c7ab3c5f1dd0170872554943575d53d6648'


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--exe', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    raw = args.exe.read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    if digest != EXPECTED:
        raise SystemExit('Unrecognized executable SHA-256; no evidence generated')
    pe = pefile.PE(data=raw)
    base = pe.OPTIONAL_HEADER.ImageBase
    reloc = {e.rva: e.type for b in pe.DIRECTORY_ENTRY_BASERELOC for e in b.entries if e.type}
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True

    def u32(rva):
        return struct.unpack('<I', pe.get_data(rva, 4))[0]

    def sec(rva):
        s = pe.get_section_by_rva(rva)
        return None if s is None else {'name': s.Name.rstrip(b'\0').decode(),
            'rva': s.VirtualAddress, 'size': s.Misc_VirtualSize,
            'raw_size': s.SizeOfRawData, 'file_offset': s.PointerToRawData,
            'characteristics': s.Characteristics}

    def ins_at(rva):
        return next(md.disasm(pe.get_data(rva, 15), base+rva, count=1), None)

    def record(i):
        return {'rva': i.address-base, 'size': i.size, 'bytes': i.bytes.hex(' '),
                'text': i.mnemonic+' '+i.op_str}

    def cfg(entry, budget=25000):
        pending, seen, unresolved, stops = [entry], {}, [], []
        while pending and len(seen) < budget:
            r = pending.pop()
            while r not in seen and len(seen) < budget:
                s = sec(r)
                if not s or not s['characteristics'] & 0x20000000:
                    stops.append({'rva': r, 'why': 'nonexecutable'}); break
                i = ins_at(r)
                if not i:
                    stops.append({'rva': r, 'why': 'decode'}); break
                seen[r] = record(i)
                if i.group(capstone.CS_GRP_RET) or i.mnemonic in ('int3', 'ud2'):
                    break
                if i.group(capstone.CS_GRP_JUMP):
                    if i.operands[0].type == capstone.x86.X86_OP_IMM:
                        pending.append(i.operands[0].imm-base)
                    else:
                        # Exact pinned Budget tooltip switch: the immediately
                        # preceding unsigned compare bounds EAX to 0..6.
                        if r == 0x2072CD and pe.get_data(0x2072C4, 3) == bytes.fromhex('83 f8 06'):
                            for n in range(7):
                                pending.append(u32(0x20B070+n*4)-base)
                        else:
                            unresolved.append(record(i))
                    if i.mnemonic == 'jmp':
                        break
                r += i.size
        return {'entry': entry, 'instructions': sorted(seen.values(), key=lambda x: x['rva']),
                'unresolved_jumps': unresolved, 'stops': stops, 'budget_exhausted': len(seen) >= budget}

    def refs(value):
        return [{'rva': r, 'section': sec(r)['name']} for r in reloc if u32(r) == value]

    report = {'schema': 1, 'capstone_version': capstone.__version__, 'pefile_version': pefile.__version__,
        'sha256': digest, 'md5': hashlib.md5(raw).hexdigest(), 'size': len(raw),
        'machine': pe.FILE_HEADER.Machine, 'timestamp': pe.FILE_HEADER.TimeDateStamp,
        'image_base': base, 'image_size': pe.OPTIONAL_HEADER.SizeOfImage,
        'dll_characteristics': pe.OPTIONAL_HEADER.DllCharacteristics,
        'relocation_count': len(reloc), 'sections': [sec(s.VirtualAddress) for s in pe.sections], 'views': []}
    for name, table, slot in VIEWS:
        address = table+slot*4
        target = u32(address)-base
        locator = u32(table-4)-base
        col = [u32(locator+i*4) for i in range(5)]
        td = col[3]-base
        decorated = pe.get_data(td+8, 160).split(b'\0')[0].decode(errors='replace')
        hierarchy = col[4]-base
        base_count, base_array = u32(hierarchy+8), u32(hierarchy+12)-base
        bases = []
        for n in range(min(base_count, 64)):
            descriptor = u32(base_array+n*4)-base
            type_rva = u32(descriptor)-base
            bases.append({'descriptor_rva': descriptor,
                'type': pe.get_data(type_rva+8, 160).split(b'\0')[0].decode(errors='replace'),
                'words': [u32(descriptor+k*4) for k in range(6)]})
        references = refs(base+table)
        for ref in references:
            r = ref['rva']
            preceding = pe.get_data(r-4096, 4096)
            offset = preceding.rfind(bytes.fromhex('55 8b ec'))
            if offset < 0:
                continue
            entry = r-4096+offset
            c = cfg(entry)
            matched = [x for x in c['instructions'] if x['rva'] <= r < x['rva']+x['size']]
            ref.update({'candidate_entry': entry, 'cfg': c,
                        'containing_instruction': matched})
        report['views'].append({'name': name, 'table': table, 'slot': slot,
            'slot_rva': address, 'target_rva': target, 'slot_relocation_type': reloc.get(address),
            'slot_file_offset': pe.get_offset_from_rva(address),
            'table_section': sec(table), 'target_section': sec(target),
            'locator_rva': locator, 'locator_words': col, 'rtti_type': decorated,
            'rtti_base_classes': bases,
            'table_refs': references, 'target_refs': refs(base+target),
            'cfg': cfg(target)})
    report['budget_switch'] = {'branch_rva': 0x2072CD,
        'bound_instructions': [record(ins_at(r)) for r in (0x2072BF, 0x2072C3, 0x2072C4, 0x2072C7)],
        'table_rva': 0x20B070,
        'targets': [{'rva': u32(0x20B070+n*4)-base, 'relocation_type': reloc.get(0x20B070+n*4)} for n in range(7)]}
    # Entries are corroborated by the manager's native baseline breakpoint
    # observations; static decoding separately establishes boundaries.
    report['observed_callers'] = []
    for label, entry, call in [('technology_slot11_creation', 0x3ABDA0, 0x3ACBC2),
                               ('selected_view_slot2', 0x26BD60, 0x26C05E),
                               ('selected_view_slot11', 0x26BD60, 0x26C06B),
                               ('selected_view_tooltip_slot10', 0x26A040, 0x26A091)]:
        c = cfg(entry)
        report['observed_callers'].append({'label': label, 'entry': entry, 'call': call,
            'return_rva': call+ins_at(call).size,
            'call_reached': any(x['rva'] == call for x in c['instructions']), 'cfg': c})
    report['observed_element_getter'] = {'cfg': cfg(0x2465D0),
        'descriptor_name_getter_cfg': cfg(0x5ED1D0),
        'element_tables': [{'table_rva': t, 'slot_rva': t+0x44,
            'target_rva': u32(t+0x44)-base, 'slot_relocation_type': reloc.get(t+0x44)}
            for t in (0xA38738, 0xA3A930)],
        'descriptor_table_rva': 0xA41D20, 'descriptor_slot_rva': 0xA41D38,
        'descriptor_target_rva': u32(0xA41D38)-base,
        'descriptor_slot_relocation_type': reloc.get(0xA41D38)}
    report['native_technology_setup'] = {'entry': 0x3ABDA0,
        'window_name': pe.get_data(0xA17B94, 64).split(b'\0')[0].decode(),
        'button_name': pe.get_data(0xA17BAC, 64).split(b'\0')[0].decode(),
        'instructions': [x for x in report['observed_callers'][0]['cfg']['instructions']
                         if 0x3AC010 <= x['rva'] <= 0x3AC084],
        'call_sites': [{'role': role, 'call_rva': call, 'vtable_byte_offset': offset,
                        'call_reached': any(x['rva'] == call for x in report['observed_callers'][0]['cfg']['instructions'])}
                       for role, call, offset in [('find_window', 0x3AC068, 0x6C),
                           ('find_child', 0x3AC076, 0x34), ('add_observer', 0x3AC084, 4)]]}
    report['observed_container_lookup'] = {'table_rva': 0xA3C0D8,
        'slots': [{'byte_offset': offset, 'target_rva': u32(0xA3C0D8+offset)-base,
            'relocation_type': reloc.get(0xA3C0D8+offset), 'cfg': cfg(u32(0xA3C0D8+offset)-base)}
            for offset in (0x6C, 0x34)]}
    report['lookup_helpers'] = [cfg(r) for r in (0x5E68A0, 0x5E6960, 0x5E6B30)]
    report['container_embedded_lookup'] = {'constructor_cfg': cfg(0x5E16C0),
        'embedded_vptr_write_rva': 0x5E19B6, 'embedded_table_rva': 0xA41E84,
        'slots': [{'byte_offset': offset, 'target_rva': u32(0xA41E84+offset)-base,
            'relocation_type': reloc.get(0xA41E84+offset), 'cfg': cfg(u32(0xA41E84+offset)-base)}
            for offset in (4, 8)]}
    report['observed_button_observable'] = {'button_table_rva': 0xA3CE50,
        'observable_table_rva': 0xA3CF78,
        'slots': [{'index': n, 'target_rva': u32(0xA3CF78+n*4)-base,
            'relocation_type': reloc.get(0xA3CF78+n*4),
            'cfg': cfg(u32(0xA3CF78+n*4)-base)} for n in (0, 1, 2)],
        'remove_node_cfg': cfg(0x1F2AE0)}
    report['production_factory_window_leads'] = {'string_rva': 0xA0EEC4,
        'string': pe.get_data(0xA0EEC4, 64).split(b'\0')[0].decode(),
        'string_refs': refs(base+0xA0EEC4),
        'native_callers': [{'entry': entry, 'call_rva': call, 'cfg': cfg(entry)}
            for entry, call in ((0x2EFDB0, 0x2EFFEF), (0x2F3E70, 0x2F44F0))]}
    report['additional_observed_descriptor'] = {'table_rva': 0xA3CACC,
        'slot_rva': 0xA3CAE4, 'target_rva': u32(0xA3CAE4)-base,
        'relocation_type': reloc.get(0xA3CAE4)}
    report['lookup_import'] = [{'dll': d.dll.decode(), 'name': i.name.decode() if i.name else None,
        'iat_rva': i.address-base} for d in pe.DIRECTORY_ENTRY_IMPORT for i in d.imports
        if i.address-base == 0x88A2C8]
    report['glue_tables'] = []
    for t in (0xA17FD8, 0xA05A24, 0xA0FF00):
        locator = u32(t-4)-base
        type_rva = u32(locator+12)-base
        report['glue_tables'].append({'rva': t,
            'rtti_type': pe.get_data(type_rva+8, 160).split(b'\0')[0].decode(),
            'slots': [{'index': n, 'target_rva': u32(t+n*4)-base,
                       'relocation_type': reloc.get(t+n*4), 'cfg': cfg(u32(t+n*4)-base)}
                      for n in (0, 2)]})
    # Decode executable sections linearly to locate candidate virtual dispatches.
    # These candidates require independent CFG entry verification before use.
    dispatches = []
    for s in pe.sections:
        if not s.Characteristics & 0x20000000:
            continue
        md.skipdata = True
        for i in md.disasm(pe.get_data(s.VirtualAddress, s.SizeOfRawData), base+s.VirtualAddress):
            if i.id and i.mnemonic in ('call', 'jmp') and i.operands and i.operands[0].type == capstone.x86.X86_OP_MEM:
                op = i.operands[0].mem
                if op.disp in (40, 44):
                    dispatches.append(record(i))
        md.skipdata = False
    report['candidate_slot_dispatches'] = dispatches
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({'output': str(args.output), 'sha256': hashlib.sha256(args.output.read_bytes()).hexdigest(),
        'views': [{k:v for k,v in x.items() if k not in ('cfg','table_refs','target_refs','locator_words','table_section','target_section','rtti_base_classes')} |
                  {'cfg_instructions': len(x['cfg']['instructions']), 'unresolved_jumps': len(x['cfg']['unresolved_jumps']),
                   'table_refs': [{k:v for k,v in t.items() if k != 'cfg'} for t in x['table_refs']]} for x in report['views']], 'dispatch_candidates': len(dispatches)}, indent=2))


if __name__ == '__main__':
    main()
