#!/usr/bin/env python3
"""Compare Logs\\v2dll_rng.log from two (or more) machines of one multiplayer game.

Usage:  python compare_rng_logs.py host\\v2dll_rng.log client1\\v2dll_rng.log [client2\\...]

Each log has one line per game day:
  <timestamp> date=1840-08-10 00:00 ctr=1892614 n=6157 h=3FA91C07 thr=1 drop=0 | 1D2F40.3=88 ...
(see ENABLE_RNG_PROBE in PATCHES.md). Lines are aligned by `date=`. For every pair
(first log vs each other) the script prints the first day where n (Random() calls) or
h (order hash of the callers) differs and the per-caller counts that differ that day.
RVA + 0x400000 = VA in Ghidra.
"""
import re
import sys

LINE = re.compile(r'date=(\S+ \S+) ctr=(-?\d+) n=(\d+) h=([0-9A-Fa-f]+) thr=(\d+) drop=(\d+) \|(.*)$')


def load(path):
    days = {}
    order = []
    with open(path, encoding='utf-8', errors='replace') as f:
        for line in f:
            m = LINE.search(line)
            if not m:
                continue
            date, ctr, n, h, thr, drop, rest = m.groups()
            callers = {}
            for tok in rest.split():
                k, _, v = tok.partition('=')
                try:
                    callers[k] = int(v)
                except ValueError:
                    pass
            # one session may restart the same date; keep the first occurrence
            if date in days:
                continue
            days[date] = dict(ctr=int(ctr), n=int(n), h=h.upper(), thr=int(thr),
                              drop=int(drop), callers=callers)
            order.append(date)
    return days, order


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    base_path = argv[1]
    base, order = load(base_path)
    print(f'{base_path}: {len(base)} days; multi-thread days: '
          f'{sum(1 for d in base.values() if d["thr"] > 1)}; dropped: {sum(d["drop"] for d in base.values())}')
    for other_path in argv[2:]:
        other, _ = load(other_path)
        print(f'\n=== {base_path}  vs  {other_path}  ({len(other)} days; multi-thread days: '
              f'{sum(1 for d in other.values() if d["thr"] > 1)})')
        common = [d for d in order if d in other]
        if not common:
            print('no common dates')
            continue
        first = None
        for d in common:
            a, b = base[d], other[d]
            if a['n'] != b['n'] or a['h'] != b['h']:
                first = d
                break
        if first is None:
            print(f'identical (n and h) on all {len(common)} common days')
            continue
        i = common.index(first)
        print(f'first difference: date={first} (day #{i + 1} of {len(common)} common days)')
        a, b = base[first], other[first]
        print(f'  n:   {a["n"]} vs {b["n"]}  (diff {b["n"] - a["n"]:+d})   h: {a["h"]} vs {b["h"]}')
        print(f'  ctr: {a["ctr"]} vs {b["ctr"]}')
        keys = sorted(set(a['callers']) | set(b['callers']))
        print('  callers whose count differs (caller RVA.variant: first / second):')
        shown = 0
        for k in keys:
            ca, cb = a['callers'].get(k, 0), b['callers'].get(k, 0)
            if ca != cb:
                print(f'    {k}: {ca} / {cb}   (Ghidra VA {int(k.split(".")[0], 16) + 0x400000:08X})'
                      if not k.startswith('ext') else f'    {k}: {ca} / {cb}')
                shown += 1
        if shown == 0:
            print('    none - same callers and counts, only the ORDER of calls differs (see h)')
        prev = common[i - 1] if i else None
        if prev:
            print(f'  previous day {prev}: n {base[prev]["n"]} vs {other[prev]["n"]}, '
                  f'h {base[prev]["h"]} vs {other[prev]["h"]}  (equal => divergence starts on {first})')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
