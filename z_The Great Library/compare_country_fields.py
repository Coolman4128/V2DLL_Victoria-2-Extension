#!/usr/bin/env python3
"""Per-day, per-country comparison of the Z lines of Logs\\v2dll_state.log (DLL >= 5.68).

Usage:  python compare_country_fields.py host\\v2dll_state.log client\\v2dll_state.log [countries.txt]

Each Z line has, per country, six 8-digit hashes: money+snapshot, industry, population, pop money
pools, state savings, factories. For every day the script lists the countries that differ and WHICH
of the six fields differ - the first such day names the field in which the simulation of host and
client first parted. countries.txt (common\\countries.txt of the mod) is optional and only used to
print tags (index 0 is the null country, so tag = line index - 1 + 1).
"""
import re
import sys

FIELDS = ('money', 'industry', 'pop', 'pools', 'savings', 'factories')


def load(path):
    days, order, cur = {}, [], None
    with open(path, encoding='utf-8', errors='replace') as f:
        for line in f:
            m = re.search(r'date=(\S+) \S+ nC=', line)
            if m:
                cur = m.group(1)
                order.append(cur)
                days[cur] = []
                continue
            m = re.match(r'^\S+ \S+ Z(.*)$', line.rstrip('\r\n'))
            if m and cur:
                days[cur] = m.group(1).split()
    return days, order


def tags(path):
    out = []
    if not path:
        return out
    for l in open(path, encoding='cp1251', errors='replace'):
        m = re.match(r'\s*([A-Z0-9]{3})\s*=\s*"', l)
        if m:
            out.append(m.group(1))
    return out


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    a, order = load(argv[1])
    b, _ = load(argv[2])
    tg = tags(argv[3]) if len(argv) > 3 else []
    shown = 0
    for d in order:
        za, zb = a.get(d, []), b.get(d, [])
        if not za or not zb:
            continue
        diff = []
        for i in range(min(len(za), len(zb))):
            x, y = za[i], zb[i]
            if x != y:
                fl = [FIELDS[k] for k in range(6) if x[k * 8:k * 8 + 8] != y[k * 8:k * 8 + 8]]
                name = tg[i - 1] if 0 < i <= len(tg) else '?'
                diff.append((i, name, fl))
        if diff:
            print(f'{d}: {len(diff)} countries differ')
            for i, name, fl in diff[:15]:
                print(f'    #{i} {name}: {",".join(fl)}')
            if len(diff) > 15:
                print(f'    ... and {len(diff) - 15} more')
            shown += 1
            if shown >= 4:
                print('(stopped after 4 differing days)')
                break
    if not shown:
        print('Z lines identical on all common days (or missing - needs DLL >= 5.68 on both machines)')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
