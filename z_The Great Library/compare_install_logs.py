#!/usr/bin/env python3
"""Compare the install block of the LAST session in several v2dll.log files.

Usage:  python compare_install_logs.py host\\v2dll.log client1\\v2dll.log [client2\\v2dll.log ...]

For multiplayer every machine must run the same DLL with the same ini/defines: a patch that is
'применён' on one machine and missing/'отключён' on another changes the simulation. The script takes
each log's last '---- Install ----' .. 'Install: done' block, strips per-run noise (pid, tid, base,
cave addresses) and prints every line that is not present in ALL logs, tagged with the log it came from.
"""
import re
import sys


def last_install_block(path):
    lines = open(path, encoding='utf-8', errors='replace').read().split('\n')
    start = None
    for i, l in enumerate(lines):
        if 'Install ----' in l:
            start = i
    if start is None:
        return None, []
    block = []
    for l in lines[start:]:
        block.append(l)
        if 'Install: done' in l:
            break
    return lines[start], block


COSMETIC = re.compile(r'; не в отпечатке|: idle rva|: mid rva|^ArmySelect:|^Heap:|^Timer:|^GoodsIcons|^Rng ?Probe[A-D]:')
PATHLINE = re.compile(r'(читаю|сканирую|не найдена|прочитан|Cfg production_types)')

NOISE = [
    (re.compile(r'^\[\d+\]\s*'), ''),
    (re.compile(r'pid=\d+ tid=\d+'), 'pid=? tid=?'),
    (re.compile(r'\bbase = [0-9A-Fa-f]+'), 'base = ?'),
    (re.compile(r'(пещера|cave)\s+[0-9A-Fa-f]{6,8}', re.I), r'\1 ?'),
    (re.compile(r'(slider|button|panel|topbar|menuOwner)=[0-9A-Fa-f]{6,8}'), r'\1=?'),
]


def norm(line):
    for rx, rep in NOISE:
        line = rx.sub(rep, line)
    if PATHLINE.search(line):
        line = re.sub(r"'[^']*'", "'<path>'", line)
        line = re.sub(r'\([^)]*\)\s*$', '', line)
    return line.strip()


SHOW_ALL = False


def main(argv):
    global SHOW_ALL
    if '--all' in argv:
        SHOW_ALL = True
        argv = [a for a in argv if a != '--all']
    if len(argv) < 3:
        print(__doc__)
        return 2
    sets = {}
    for p in argv[1:]:
        head, block = last_install_block(p)
        if head is None:
            print(f'{p}: no "Install ----" line found')
            continue
        ver = re.search(r'версия (\S+)', head)
        print(f'{p}: DLL {ver.group(1) if ver else "?"}, install block {len(block)} lines')
        sets[p] = [norm(l) for l in block if norm(l) and (SHOW_ALL or not COSMETIC.search(norm(l)))]
    if len(sets) < 2:
        return 1
    everything = set()
    for v in sets.values():
        everything.update(v)
    common = set.intersection(*[set(v) for v in sets.values()])
    diff = sorted(everything - common)
    print()
    if not diff:
        print('install blocks are IDENTICAL (after removing pid/base/cave noise)')
        return 0
    print(f'{len(diff)} line(s) differ:')
    for l in diff:
        who = [p for p, v in sets.items() if l in v]
        print(f'  [{", ".join(p.split(chr(92))[-1] for p in who)}] {l}')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
