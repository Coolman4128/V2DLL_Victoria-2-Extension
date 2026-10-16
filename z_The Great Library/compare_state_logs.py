#!/usr/bin/env python3
"""Compare Logs\\v2dll_state.log from two machines of one multiplayer game (ENABLE_STATE_PROBE).

Usage:  python compare_state_logs.py [--mod <mod\\2 folder>] host_state.log client_state.log

Each game day has 4 lines (the date is the state at the entry of the daily tick):
  date=.. nC nProv nPop nState nBld hc hs hb hq hp hm   group hashes
  C <hash per country>      country money/snapshot/industry/population + its states and factories
                            + population money pools (3 strata x pop type)
  P <hash per province>     all pops of the province (two int64 fields per pop)
  M <market prices> | <market demand>   raw int64 hex
The script aligns days by date, finds the FIRST day where any group differs and prints which
groups, which country / province indices differ (country index = position in common\\countries.txt
order, province index = province id) and which market slots. With --mod it prints tags and names.
"""
import os
import re
import sys

HEAD = re.compile(r'date=(\S+ \S+) nC=(\d+) nProv=(\d+) nPop=(\d+) nState=(\d+) nBld=(\d+) '
                  r'hc=(\w+) hs=(\w+) hb=(\w+) hq=(\w+) hp=(\w+) hm=(\w+)')
GROUPS = ('hc', 'hs', 'hb', 'hq', 'hp', 'hm')
GROUP_INFO = {
    'hc': 'country money / snapshot / industry / population',
    'hs': 'state savings (+0x228)',
    'hb': 'factories (+0x210/+0x218)',
    'hq': 'population money pools (country x strata x pop type)',
    'hp': 'pops by province (+0x180/+0x250)',
    'hm': 'market prices + demand',
}


def load(path):
    days = {}
    order = []
    cur = None
    with open(path, encoding='utf-8', errors='replace') as f:
        for line in f:
            line = line.rstrip('\n')
            body = line[24:] if len(line) > 24 and line[4] == '-' else line
            m = HEAD.search(body)
            if m:
                date = m.group(1)
                cur = dict(counts=tuple(int(x) for x in m.groups()[1:6]),
                           h=dict(zip(GROUPS, m.groups()[6:])), C=[], P=[], M=([], []))
                if date not in days:
                    days[date] = cur
                    order.append(date)
                else:
                    cur = None
                continue
            if cur is None:
                continue
            if body.startswith('C '):
                cur['C'] = body[2:].split()
            elif body.startswith('P '):
                cur['P'] = body[2:].split()
            elif body.startswith('M '):
                left, _, right = body[2:].partition('|')
                cur['M'] = (left.split(), right.split())
    return days, order


def country_tags(mod):
    tags = []
    p = os.path.join(mod, 'common', 'countries.txt')
    try:
        for l in open(p, encoding='cp1251', errors='replace'):
            m = re.match(r'^([A-Z]{3})\s*=', l.strip())
            if m:
                tags.append(m.group(1))
    except OSError:
        pass
    return tags


def province_names(mod):
    names = {}
    root = os.path.join(mod, 'history', 'provinces')
    for r, _, fs in os.walk(root):
        for fn in fs:
            m = re.match(r'^(\d+)\s*-\s*(.*)\.txt$', fn)
            if m:
                names[int(m.group(1))] = m.group(2)
    return names


def diff_idx(a, b):
    n = max(len(a), len(b))
    return [i for i in range(n) if (a[i] if i < len(a) else None) != (b[i] if i < len(b) else None)]


def main(argv):
    mod = None
    if '--mod' in argv:
        i = argv.index('--mod')
        mod = argv[i + 1]
        argv = argv[:i] + argv[i + 2:]
    if len(argv) != 3:
        print(__doc__)
        return 2
    a, order = load(argv[1])
    b, _ = load(argv[2])
    tags = country_tags(mod) if mod else []
    pnames = province_names(mod) if mod else {}
    common = [d for d in order if d in b]
    print(f'{argv[1]}: {len(a)} days;  {argv[2]}: {len(b)} days;  common: {len(common)}')
    if not common:
        return 1
    first = None
    for d in common:
        if a[d]['h'] != b[d]['h'] or a[d]['counts'] != b[d]['counts']:
            first = d
            break
    if first is None:
        print('identical on all common days (all group hashes and counts)')
        return 0
    i = common.index(first)
    print(f'\nFIRST DIFFERENCE: {first}  (day #{i + 1} of {len(common)})')
    da, db = a[first], b[first]
    print(f'  counts nC/nProv/nPop/nState/nBld: {da["counts"]}  vs  {db["counts"]}')
    for g in GROUPS:
        if da['h'][g] != db['h'][g]:
            print(f'  DIFF {g}: {da["h"][g]} vs {db["h"][g]}   ({GROUP_INFO[g]})')
    ci = diff_idx(da['C'], db['C'])
    if ci:
        lab = lambda k: f'{k}' + (f'({tags[k]})' if k < len(tags) else '')
        print(f'  countries that differ ({len(ci)}): ' + ', '.join(lab(k) for k in ci[:40]) + (' ...' if len(ci) > 40 else ''))
    pi = diff_idx(da['P'], db['P'])
    if pi:
        lab = lambda k: f'{k}' + (f'({pnames[k]})' if k in pnames else '')
        print(f'  provinces that differ ({len(pi)}): ' + ', '.join(lab(k) for k in pi[:40]) + (' ...' if len(pi) > 40 else ''))
    mp = diff_idx(da['M'][0], db['M'][0])
    md = diff_idx(da['M'][1], db['M'][1])
    if mp:
        print(f'  market PRICE slots that differ: {mp[:30]}  (slot -> good via goods order; first: {da["M"][0][mp[0]] if mp[0] < len(da["M"][0]) else "-"} vs {db["M"][0][mp[0]] if mp[0] < len(db["M"][0]) else "-"})')
    if md:
        print(f'  market DEMAND slots that differ: {md[:30]}')
    if i:
        prev = common[i - 1]
        print(f'\n  previous day {prev}: all hashes equal = {a[prev]["h"] == b[prev]["h"]}')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
