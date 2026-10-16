#!/usr/bin/env python3
"""Compare Logs\\v2dll_watch.log (money watch, DLL >= 5.69) of two machines.

Usage:  python compare_watch_logs.py host\\v2dll_watch.log client\\v2dll_watch.log

The DLL puts a hardware write breakpoint on the treasury (+0xE78) of the watched country (KUK) on
all threads and logs, once a day, every write: the instruction after the write (RVA), up to three
return addresses (RVA) and the new low DWORD of the treasury. A line looks like
    KUK 0087410.0048C2F.0068F4C0 lo=36CEADD7 hi=00000000
RVA + 0x400000 = Ghidra VA. The script walks the days of both logs, prints the first day whose lists
differ and the first differing write with its neighbours; with raw money units (15 fraction bits)
converted to money. Ghidra: look at the function that contains the 'eip' and at its callers.
"""
import re
import sys


def load(path):
    days, order, cur = {}, [], None
    with open(path, encoding='utf-8', errors='replace') as f:
        for line in f:
            line = line.rstrip('\r\n')
            m = re.match(r'^W date=(\S+ \S+) writes=(\d+) dropped=(\d+)', line)
            if m:
                cur = m.group(1)
                order.append(cur)
                days[cur] = []
                continue
            m = re.match(r'^\s+(\S+) (\w+)\.(\w+)\.(\w+)\.(\w+) lo=(\w+) hi=(\w+)', line)
            if m and cur:
                days[cur].append(m.groups())
    return days, order


def money(lo, hi):
    v = (int(hi, 16) << 32) | int(lo, 16)
    if v >= 1 << 63:
        v -= 1 << 64
    return v / 32768.0


def fmt(e):
    return f'{e[0]} eip={e[1]} ret={e[2]}.{e[3]}.{e[4]} lo={e[5]} hi={e[6]}'


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    a, order = load(argv[1])
    b, _ = load(argv[2])
    common = [d for d in order if d in b]
    print(f'{len(order)} days in first log, {len(b)} in second, {len(common)} common')
    for d in common:
        la, lb = a[d], b[d]
        if la == lb:
            continue
        print(f'\nFIRST DIFFERENT DAY {d}: {len(la)} writes vs {len(lb)}')
        n = 0
        for i in range(max(len(la), len(lb))):
            ea = la[i] if i < len(la) else None
            eb = lb[i] if i < len(lb) else None
            if ea != eb:
                lo = max(0, i - 3)
                print('  context (first log):')
                for j in range(lo, min(len(la), i + 4)):
                    mark = '>>' if j == i else '  '
                    print(f'   {mark} {fmt(la[j])}')
                print('  context (second log):')
                for j in range(lo, min(len(lb), i + 4)):
                    mark = '>>' if j == i else '  '
                    print(f'   {mark} {fmt(lb[j])}')
                break
        return 0
    print('identical on all common days')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
