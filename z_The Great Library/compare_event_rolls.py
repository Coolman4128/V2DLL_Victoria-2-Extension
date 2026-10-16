#!/usr/bin/env python3
"""Compare the event-roll lines (E / F) of Logs\\v2dll_rng.log from two or more machines.

Usage:  python compare_event_rolls.py host\\v2dll_rng.log client1\\v2dll_rng.log [client2\\...]

Written by the event-roll probe (5.63, part of ENABLE_RNG_PROBE, see PATCHES.md). One pair
of lines per game day (one machine = one log):
  E date=1849-09-25 00:00 rolls=412 fired=1 h=1A2B3C4D drop=0 | 1100.5=40/0 1100.6=3/1:0.0.5.6.7 ...
  F date=1849-09-25 00:00 1100.6 scope=.. chance=.. rand=..          (one per event that fired)
`rolls` = number of event MTTH dice rolls that day (each costs one Random() draw = +1 to the
MP checksum counter), `h` = order hash of (mode, event id, scope, chance, rand), token
<mode><id0>.<id1>=<rolls>/<fired>[:<scope words, only for events rolled <= 4 times>].
mode 1 = country events (odd days), mode 2 = province events (even days).

For every pair (first log vs each other) prints the first day where rolls / fired / h
differ and which events were rolled a different number of times - an event whose roll count
differs is one whose trigger evaluated differently on the two machines.
"""
import re
import sys

HEAD = re.compile(r'\bE date=(\S+ \S+) rolls=(\d+) fired=(\d+) h=([0-9A-Fa-f]+) drop=(\d+) \|(.*)$')
FIRE = re.compile(r'\bF date=(\S+ \S+) (\S+) scope=(\S+) chance=(\d+) rand=(\d+)(?: hum=(-?\d+))?')
TICK = re.compile(r'\bT date=(\S+ \S+) mtidx=(\d+) mth=([0-9A-Fa-f]+) nhum=(\d+) hum=(\S*)')


def load(path):
    days = {}
    order = []
    fired = {}
    ticks = {}
    with open(path, encoding='utf-8', errors='replace') as f:
        for line in f:
            m = HEAD.search(line)
            if m:
                date, rolls, nf, h, drop, rest = m.groups()
                if date in days:       # one session may restart the same date; keep the first
                    continue
                ev = {}
                for tok in rest.split():
                    key, _, tail = tok.partition('=')
                    cnt, _, scopes = tail.partition(':')
                    r, _, fi = cnt.partition('/')
                    try:
                        ev[key] = (int(r), int(fi), scopes)
                    except ValueError:
                        pass
                days[date] = dict(rolls=int(rolls), fired=int(nf), h=h.upper(), drop=int(drop), ev=ev)
                order.append(date)
                continue
            m = FIRE.search(line)
            if m:
                date, key, scope, chance, rnd, hum = m.groups()
                fired.setdefault(date, []).append((key, scope, int(chance), int(rnd), hum))
                continue
            m = TICK.search(line)
            if m and m.group(1) not in ticks:
                ticks[m.group(1)] = (int(m.group(2)), m.group(3).upper(), int(m.group(4)), m.group(5))
    return days, order, fired, ticks


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    base_path = argv[1]
    base, order, bfired, bticks = load(base_path)
    print(f'{base_path}: {len(base)} days with event-roll lines; '
          f'total rolls {sum(d["rolls"] for d in base.values())}; dropped {sum(d["drop"] for d in base.values())}')
    for other_path in argv[2:]:
        other, _, ofired, oticks = load(other_path)
        print(f'\n=== {base_path}  vs  {other_path}  ({len(other)} days)')
        common = [d for d in order if d in other]
        if not common:
            print('no common dates (does the second log carry E lines? needs DLL >= 5.63)')
            continue
        td = [d for d in order if d in bticks and d in oticks]
        if td:
            bad = [d for d in td if bticks[d][:2] != oticks[d][:2]]
            hbad = [d for d in td if bticks[d][3] != oticks[d][3]]
            print(f'  T lines (MT state / humans): {len(td)} common days; MT index/hash differ on {len(bad)} '
                  f'(first: {bad[0] if bad else "-"}); human list differs on {len(hbad)} '
                  f'(first: {hbad[0] if hbad else "-"})')
            if bad:
                d = bad[0]
                print(f'    {d}: mtidx {bticks[d][0]} vs {oticks[d][0]}  mth {bticks[d][1]} vs {oticks[d][1]}')
            if hbad:
                d = hbad[0]
                print(f'    {d}: humans first : {bticks[d][3]}')
                print(f'    {d}: humans second: {oticks[d][3]}')
        else:
            print('  (no T lines in one of the logs - needs DLL >= 5.64)')
        first = None
        for d in common:
            a, b = base[d], other[d]
            if a['rolls'] != b['rolls'] or a['fired'] != b['fired'] or a['h'] != b['h']:
                first = d
                break
        if first is None:
            print(f'identical (rolls, fired, h) on all {len(common)} common days')
            continue
        i = common.index(first)
        a, b = base[first], other[first]
        print(f'first difference: date={first} (day #{i + 1} of {len(common)} common days)')
        print(f'  rolls: {a["rolls"]} vs {b["rolls"]} (diff {b["rolls"] - a["rolls"]:+d})   '
              f'fired: {a["fired"]} vs {b["fired"]}   h: {a["h"]} vs {b["h"]}')
        print('  events whose roll/fire counts differ (key = <mode><id0>.<id1>: first / second):')
        shown = 0
        for k in sorted(set(a['ev']) | set(b['ev'])):
            ea = a['ev'].get(k, (0, 0, ''))
            eb = b['ev'].get(k, (0, 0, ''))
            if ea[:2] != eb[:2]:
                print(f'    {k}: {ea[0]}/{ea[1]} / {eb[0]}/{eb[1]}')
                if ea[2] or eb[2]:
                    print(f'       scopes (w0.w1.w2.w3.idx): {ea[2] or "-"}  |  {eb[2] or "-"}')
                shown += 1
        if shown == 0:
            print('    none - same events rolled the same number of times; the difference is in the '
                  'order or in scope/chance/rand values (compare the scopes / F lines)')
        fa, fb = bfired.get(first, []), ofired.get(first, [])
        if fa != fb:
            print('  fired events differ:')
            for x in fa:
                if x not in fb:
                    print(f'    only first : {x}')
            for x in fb:
                if x not in fa:
                    print(f'    only second: {x}')
        if i:
            prev = common[i - 1]
            print(f'  previous day {prev}: rolls {base[prev]["rolls"]} vs {other[prev]["rolls"]}, '
                  f'h {base[prev]["h"]} vs {other[prev]["h"]}  (equal => divergence starts on {first})')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
