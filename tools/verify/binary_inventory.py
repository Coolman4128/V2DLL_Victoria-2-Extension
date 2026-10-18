"""Read-only PE/source inventory; observations are not an ABI certification.

Requires pefile and capstone. Output contains private executable bytes; keep it
outside the repository. Conservative lexical extraction keeps unresolved calls.
"""
import argparse
import bisect
import hashlib
import json
import re
from pathlib import Path

import capstone
import pefile


def stripped(text):
    pattern = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/'
    return re.sub(pattern, lambda m: re.sub(r'[^\n]', ' ', m[0]) if m[0].startswith(('//', '/*')) else m[0], text)


def split_args(text):
    result, begin, depth, quote = [], 0, 0, False
    for i, ch in enumerate(text):
        if ch == '"' and (not i or text[i-1] != '\\'):
            quote = not quote
        if quote:
            continue
        if ch in '({[':
            depth += 1
        elif ch in ')}]':
            depth -= 1
        elif ch == ',' and depth == 0:
            result.append(text[begin:i].strip()); begin = i+1
    result.append(text[begin:].strip())
    return result


def closing(text, start, opening='(', close=')'):
    depth = 1
    tokens = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|'+re.escape(opening)+'|'+re.escape(close)
    for token in re.finditer(tokens, text[start+1:]):
        if token[0] == opening:
            depth += 1
        if token[0] == close:
            depth -= 1
        if not depth:
            return start+1+token.start()
    raise ValueError('unclosed expression')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    raw = args.exe.read_bytes(); pe = pefile.PE(data=raw)
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32); md.detail = True
    report = {'schema': 1, 'exe': str(args.exe), 'sha256': hashlib.sha256(raw).hexdigest(),
              'md5': hashlib.md5(raw).hexdigest(), 'machine': pe.FILE_HEADER.Machine,
              'image_base': pe.OPTIONAL_HEADER.ImageBase, 'sections': [], 'sources': [],
              'signatures': [], 'calls': [], 'address_leads': [], 'byte_patches': [], 'table_sites': []}
    for s in pe.sections:
        report['sections'].append({'name': s.Name.rstrip(b'\0').decode(), 'rva': s.VirtualAddress,
                                  'virtual_size': s.Misc_VirtualSize, 'file_offset': s.PointerToRawData,
                                  'raw_size': s.SizeOfRawData, 'characteristics': s.Characteristics})

    def inspect(rva, n):
        sec = next((s for s in pe.sections if s.VirtualAddress <= rva < s.VirtualAddress + max(s.Misc_VirtualSize, s.SizeOfRawData)), None)
        if sec is None:
            return {'mapped': False}
        delta = rva - sec.VirtualAddress
        out = {'mapped': True, 'section': sec.Name.rstrip(b'\0').decode(), 'executable': bool(sec.Characteristics & 0x20000000), 'file_backed': delta+n <= sec.SizeOfRawData}
        if not out['file_backed']:
            return out
        data = pe.get_data(rva, max(n, 16))
        out['actual'] = data[:n].hex(' '); out['file_offset'] = sec.PointerToRawData + delta
        if out['executable']:
            ins = list(md.disasm(data, pe.OPTIONAL_HEADER.ImageBase+rva))
            stolen = [i for i in ins if i.address < pe.OPTIONAL_HEADER.ImageBase+rva+n]
            out['decode'] = [{'rva': i.address-pe.OPTIONAL_HEADER.ImageBase, 'length': i.size, 'text': i.mnemonic+' '+i.op_str,
                              'relative_control': bool((i.group(capstone.CS_GRP_JUMP) or i.group(capstone.CS_GRP_CALL)) and any(o.type == capstone.x86.X86_OP_IMM for o in i.operands))} for i in stolen]
            out['ends_on_decoded_boundary'] = bool(stolen and stolen[-1].address+stolen[-1].size == pe.OPTIONAL_HEADER.ImageBase+rva+n)
            out['relative_control_in_span'] = any(i['relative_control'] for i in out['decode'])
        return out

    for path in sorted(args.source_root.rglob('*')):
        if path.suffix not in ('.cpp', '.h', '.c') or any(x in path.parts for x in ('Release', 'Debug', 'x64', 'minhook', 'MinHook')):
            continue
        original = path.read_text(encoding='utf-8-sig'); src = stripped(original)
        rel = str(path.relative_to(args.source_root)); report['sources'].append({'path': rel, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
        scope_positions, scopes, stack = [-1], [()], []
        for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]', src):
            if token[0] == '{':
                stack.append(token.start())
            elif token[0] == '}':
                if stack:
                    stack.pop()
            else:
                continue
            scope_positions.append(token.start()); scopes.append(tuple(stack))

        def accessible(declaration, use):
            ds = scopes[bisect.bisect_left(scope_positions, declaration)-1]
            us = scopes[bisect.bisect_left(scope_positions, use)-1]
            return us[:len(ds)] == ds

        functions = []
        for fm in re.finditer(r'^\s*(?:(?:static|inline|const|unsigned|bool|void|int|char|DWORD|long|__\w+)\s+)+[\w* :<>]*?\b(\w+)\s*\([^;{}]*\)\s*\{', src, re.M):
            brace = src.index('{', fm.start())
            functions.append((fm.start(), closing(src, brace, '{', '}'), fm[1]))
        dormant = [(m.start(), closing(src, src.index('{', m.start()), '{', '}')) for m in re.finditer(r'\bif\s*\(\s*false\s*\)\s*\{', src)]
        definitions, arrays = [], []
        for m in re.finditer(r'\b(?:DWORD|int|unsigned|size_t)\s+(\w+)\s*=\s*(0x[\da-fA-F]+|\d+)\s*[,;]', src):
            definitions.append((m.start(), m[1], int(m[2], 0)))
        for m in re.finditer(r'\bconst\s+(?:DWORD|int|unsigned|size_t)\s+([^;{}]+);', src):
            for entry in re.finditer(r'\b(\w+)\s*=\s*(0x[\da-fA-F]+|\d+)\s*(?=,|$)', m[1]):
                item = (m.start()+m[0].index(m[1])+entry.start(), entry[1], int(entry[2], 0))
                if not any(d[1:] == item[1:] and abs(d[0]-item[0]) < 20 for d in definitions):
                    definitions.append(item)
        definitions.sort()
        for m in re.finditer(r'\b(?:unsigned\s+char|BYTE|char)\s+(\w+)\s*\[([^]]*)\]\s*=\s*\{([^{}]*)\}', src):
            try:
                vals = [int(v.strip(), 0) for v in m[3].split(',') if v.strip()]
                if any(v < 0 or v > 255 for v in vals):
                    continue
                count = int(m[2], 0) if m[2].strip() else len(vals)
                arrays.append((m.start(), m[1], bytes(vals + [0] * (count-len(vals)))))
            except ValueError:
                continue

        def value(expr, pos):
            expr = expr.strip()
            try:
                return int(expr, 0)
            except ValueError:
                pass
            if expr.startswith('sizeof('):
                b = array(expr[7:-1], pos)
                return len(b) if b is not None else None
            # Generic helper parameters must never bind to an unrelated local.
            if expr in ('rva', 'slot', 'n', 'len', 'steal', 'slotIndex'):
                return None
            matches = [d for d in definitions if d[0] < pos and d[1] == expr and accessible(d[0], pos)]
            return matches[-1][2] if matches else None

        def array(expr, pos):
            matches = [d for d in arrays if d[0] < pos and d[1] == expr.strip() and accessible(d[0], pos)]
            if not matches:
                return None
            binding = matches[-1]
            # Runtime-patched/rebased arrays are dependencies, not literal checks.
            interval = src[src.index('}', binding[0])+1:pos]
            name = re.escape(expr.strip())
            if re.search(r'\b'+name+r'\s*\[[^]]+\]\s*=|\b(?:memcpy|memset)\s*\(\s*'+name+r'\b|\*\s*\([^;\n]*\b'+name+r'\s*\+[^;\n]*=', interval):
                return None
            return binding[2]

        def pointer(expr, pos):
            plus = re.fullmatch(r'(\w+)\s*\+\s*(\d+|0x[\da-fA-F]+)', expr.strip())
            if plus and plus[1] != 'g_base':
                base = pointer(plus[1], pos)
                return base + int(plus[2], 0) if base is not None else None
            m = re.search(r'g_base\s*\+\s*(0x[\da-fA-F]+|\w+)', expr)
            if m:
                return value(m[1], pos)
            name = expr.strip()
            assignments = [a for a in re.finditer(r'\b'+re.escape(name)+r'\s*=\s*([^;]+);', src[:pos]) if accessible(a.start(), pos)] if re.fullmatch(r'\w+', name) else []
            return pointer(assignments[-1][1], assignments[-1].start()) if assignments else None

        for pos, name, number in definitions:
            if number >= 0x10000 and re.search(r'RVA|Site|SITE|Slot|SLOT|Call|CALL|Vtable|VTABLE|Resume|RESUME|Target|TARGET|Fn$|HOOK|ENTRY|REJECT|TAIL', name):
                report['address_leads'].append({'source': rel, 'line': src.count('\n', 0, pos)+1, 'name': name, 'rva': number, 'observation': inspect(number, 1), 'status': 'address lead only; no expected bytes or ABI proof'})

        names = 'StealToTrampoline|PlantMidJump|PatchBytes|PatchE8|PatchSlot|PatchVtableSlot|memcmp|VirtualProtect|MH_CreateHook|WriteJmpSite|PatchImm8Sleep|PatchFilterPredCall|PatchEvRollSite|InstallPopSite|PatchHideGuiCall|HookIat|HookIatOrdinal'
        for m in re.finditer(r'\b('+names+r')\s*\(', src):
            end = closing(src, src.index('(', m.start())); callargs = split_args(src[src.index('(', m.start())+1:end])
            if src[end+1:].lstrip().startswith('{'):
                continue
            row = {'source': rel, 'line': src.count('\n', 0, m.start())+1, 'kind': m[1], 'arguments': callargs,
                   'status': 'unresolved lexical record; reachability/activation not inferred'}
            owners = [f for f in functions if f[0] <= m.start() <= f[1]]
            row['owner'] = owners[-1][2] if owners else None
            row['activation'] = 'dormant literal if(false)' if any(a <= m.start() <= b for a, b in dormant) else 'requires settings/callgraph review'
            rva, expect, n = None, None, None
            if m[1] == 'PatchEvRollSite' and len(callargs) >= 2:
                rva, expect, n = value(callargs[0], m.start()), array(callargs[1], m.start()), 6
            if m[1] == 'WriteJmpSite' and len(callargs) >= 7:
                rva, expect, n = value(callargs[0], m.start()), array(callargs[1], m.start()), value(callargs[2], m.start())
                overwrite = value(callargs[3], m.start())
                row['overwrite_length'] = overwrite
                if rva is not None and overwrite is not None:
                    row['overwrite_observation'] = inspect(rva, overwrite)
                resume, rexp, rn = value(callargs[4], m.start()), array(callargs[5], m.start()), value(callargs[6], m.start())
                row['resume_rva'] = resume
                if resume is not None and rn is not None and rn > 0:
                    row['resume_observation'] = inspect(resume, rn)
                    if rexp is not None:
                        row['resume_match'] = pe.get_data(resume, rn) == rexp[:rn]
                row['complete_overwrite_signature'] = n is not None and n == overwrite
            if m[1] in ('StealToTrampoline', 'PlantMidJump') and len(callargs) > 2:
                rva, n = value(callargs[0], m.start()), value(callargs[1], m.start())
                expect = array(callargs[4 if m[1] == 'StealToTrampoline' else 2], m.start())
            elif m[1] in ('PatchBytes', 'PatchE8') and len(callargs) >= 4:
                rva = value(callargs[0], m.start()); expect = array(callargs[1], m.start())
                n = 5 if m[1] == 'PatchE8' else value(callargs[3], m.start())
            elif m[1] == 'memcmp' and len(callargs) == 3:
                rva, expect, n = pointer(callargs[0], m.start()), array(callargs[1], m.start()), value(callargs[2], m.start())
            elif m[1] == 'PatchSlot' and len(callargs) >= 2:
                base, slot = value(callargs[0], m.start()), value(callargs[1], m.start())
                if base is not None and slot is not None:
                    rva, n = base+4*slot, 4
            elif m[1] == 'VirtualProtect' and len(callargs) >= 2:
                rva, n = pointer(callargs[0], m.start()), value(callargs[1], m.start())
            if rva is not None:
                row['rva'] = rva
            if n is not None:
                row['length'] = n
            if rva is not None and n is not None and 0 < n <= 64:
                row['observation'] = inspect(rva, n)
                if expect is not None and len(expect) >= n:
                    row['expected'] = expect[:n].hex(' ')
                    ei = 4 if m[1] == 'StealToTrampoline' else 2 if m[1] == 'PlantMidJump' else 1
                    bindings = [d for d in arrays if d[0] < m.start() and d[1] == callargs[ei] and accessible(d[0], m.start())]
                    if bindings:
                        row['expected_definition_line'] = src.count('\n', 0, bindings[-1][0])+1
                    row['match'] = pe.get_data(rva, n) == expect[:n]
                    row['status'] = 'source expected bytes compared; ABI/activation unproved'
                    report['signatures'].append(row)
            report['calls'].append(row)

        # Every base-relative source access remains discoverable even when it is
        # a native call/data dependency or uses an unsupported installer shape.
        for m in re.finditer(r'\bg_base\s*\+\s*(0x[\da-fA-F]+|\w+)', src):
            number = value(m[1], m.start())
            report['address_leads'].append({'source': rel, 'line': src.count('\n', 0, m.start())+1,
                'name': m[1], 'rva': number, 'observation': inspect(number, 1) if number is not None else None,
                'status': 'base-relative access; no ABI/activation proof'})

        if path.name == 'V2TechButton.cpp':
            for m in re.finditer(r'\{\s*"([^"]+)"\s*,\s*(0x[\da-fA-F]+)\s*,\s*\{\s*((?:0x[\da-fA-F]+\s*,?\s*)+)\}\s*,\s*(true|false)\s*\}', src):
                expected = bytes(int(x.strip(), 0) for x in m[3].split(',') if x.strip())
                rva = int(m[2], 0)
                report['table_sites'].append({'kind': 'PopSite', 'name': m[1], 'rva': rva, 'expected': expected.hex(' '),
                    'line': src.count('\n', 0, m.start())+1, 'compiled_enabled': m[4] == 'true', 'match': pe.get_data(rva, len(expected)) == expected,
                    'observation': inspect(rva, len(expected)), 'activation': 'table exists; no InstallPopSite caller found by source search'})
            views = re.search(r'VIEWS\[\]\s*=\s*\{([\s\S]*?)\n\};', src)[1]
            for m in re.finditer(r'\{\s*"([^"]+)"\s*,\s*(0x[\da-fA-F]+)\s*,\s*(\d+)\s*,', views):
                rva = int(m[2], 0)+4*int(m[3])
                report['table_sites'].append({'kind': 'ViewDef vtable', 'name': m[1], 'rva': rva, 'slot_index': int(m[3]),
                    'observation': inspect(rva, 4), 'activation': 'unconditional root Install loop', 'status': 'no source expected target; unsupported pending profile proof'})
            table = re.search(r'EXE_PATCHES\[\]\s*=\s*\{([\s\S]*?)\n\};', src)[1]
            for m in re.finditer(r'\{\s*"([^"]+)"\s*,\s*(0x[\da-fA-F]+|0)\s*,\s*(0x[\da-fA-F]+|0)\s*,\s*(\d+)\s*,\s*\{([^{}]+)\}\s*,\s*\{([^{}]+)\}\s*,\s*(true|false)\s*\}', table):
                off, rva, n = int(m[2], 0), int(m[3], 0), int(m[4])
                if not rva:
                    rva = pe.get_rva_from_offset(off)
                expected, replacement = (bytes(int(x.strip(), 0) for x in m[k].split(',') if x.strip()) for k in (5, 6))
                actual = pe.get_data(rva, n)
                report['byte_patches'].append({'name': m[1], 'rva': rva, 'file_offset_lead': off, 'length': n,
                    'compiled_enabled': m[7] == 'true', 'expected': expected.hex(' '), 'replacement': replacement.hex(' '),
                    'match': actual == expected, 'already_replacement': actual == replacement, 'observation': inspect(rva, n)})

    report['limits'] = ['Lexical extraction is not a C++ compiler; unresolved records deliberately remain.',
        'Linear decode from each site tests span length only, not a trusted function-entry/control-flow boundary.',
        'Brace-scoped preceding bindings prevent cross-function fallback, but macros/templates and dataflow still need manual review.',
        'No upstream reference executable supplied: hash difference is known, full binary diff unavailable.',
        'No native calling convention, register/FPU, lifetime, thread or safe-phase certification is implied.',
        'Compiled defaults and dynamic settings are not interpreted; installer activation needs independent review.']
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps({'output': str(args.output), 'sha256': hashlib.sha256(args.output.read_bytes()).hexdigest(),
        'signatures': len(report['signatures']), 'signature_mismatches': [r for r in report['signatures'] if not r['match']],
        'byte_patches': len(report['byte_patches']), 'byte_patch_mismatches': [r for r in report['byte_patches'] if not r['match']],
        'calls': len(report['calls']), 'address_leads': len(report['address_leads'])}, indent=2))


if __name__ == '__main__':
    main()
