#!/usr/bin/env python3
"""Compare the detailed state dump (D lines of Logs\\v2dll_state.log, DLL >= 5.66) of two machines.

Usage:  python compare_state_detail.py host\\v2dll_state.log client\\v2dll_state.log [date]

The DLL writes raw values for the first 3 days of a session (see StateDetailDump in the source):
  D C <i> <TAG> m= s= n= p=    country money / snapshot / industry (int64, 15 fraction bits) / population
  D Q <i> <k>: ...             population money pools of country i, stratum k, per pop-type id
  D S <i> ...                  state savings of country i
  D B <i> ...                  factory values <+0x210>:<+0x218> of country i
  D P <prov> <bucket> <170 DWORD>   raw pops of the first provinces
For every day present in both logs prints, per section, how many values differ and the first
differences with their size, so you can see WHICH field starts to differ and by how much.
"""
import re
import sys

LINE_PREFIX = re.compile(r'^(?:\d{4}-\d\d-\d\d \d\d:\d\d:\d\d\.\d+ )?(D .*)$')


def load(path):
    days = {}
    cur = None
    with open(path, encoding='utf-8', errors='replace') as f:
        for raw in f:
            raw = raw.rstrip('\r\n')
            m = LINE_PREFIX.match(raw)
            if not m:
                continue
            line = m.group(1)
            if line.startswith('D date='):
                cur = line[len('D date='):].strip()
                days[cur] = {'C': {}, 'Q': {}, 'S': {}, 'B': {}, 'P': {}}
                continue
            if cur is None:
                continue
            parts = line.split()
            kind = parts[1] if len(parts) > 1 else ''
            if kind == 'C' and len(parts) >= 8:
                days[cur]['C'][int(parts[2])] = dict(tag=parts[3], **{kv.split('=')[0]: kv.split('=')[1] for kv in parts[4:8]})
            elif kind == 'Q':
                key = (int(parts[2]), parts[3].rstrip(':'))
                days[cur]['Q'][key] = parts[4:]
            elif kind == 'S':
                days[cur]['S'][int(parts[2])] = parts[3:]
            elif kind == 'B':
                days[cur]['B'][int(parts[2])] = parts[3:]
            elif kind == 'P' and len(parts) > 4:
                key = (int(parts[2]), int(parts[3]))
                days[cur]['P'].setdefault(key, []).append(parts[4:])
    return days


def s64(h):
    v = int(h, 16)
    return v - (1 << 64) if v >= (1 << 63) else v


def fx(h):
    try:
        return '%.3f' % (s64(h) / 32768.0)
    except ValueError:
        return h


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    a = load(argv[1])
    b = load(argv[2])
    dates = [d for d in a if d in b]
    if len(argv) > 3:
        dates = [d for d in dates if d.startswith(argv[3])]
    if not dates:
        print('no common days with D blocks (needs DLL >= 5.66 on both machines)')
        return 1
    for d in dates:
        A, B = a[d], b[d]
        print(f'\n=========== {d}')
        # countries
        for fld in ('m', 's', 'n', 'p'):
            diff = [i for i in A['C'] if i in B['C'] and A['C'][i][fld] != B['C'][i][fld]]
            print(f'  countries field {fld}: {len(diff)} of {len(A["C"])} differ')
            for i in diff[:12]:
                x, y = A['C'][i][fld], B['C'][i][fld]
                if fld == 'p':
                    print(f'     #{i} {A["C"][i]["tag"]}: {int(x, 16)} vs {int(y, 16)}')
                else:
                    print(f'     #{i} {A["C"][i]["tag"]}: {fx(x)} vs {fx(y)}  (diff {(s64(x) - s64(y)) / 32768.0:+.4f})')
        # pools
        keys = [k for k in A['Q'] if k in B['Q']]
        n = 0
        for k in keys:
            if A['Q'][k] != B['Q'][k]:
                n += 1
                if n <= 6:
                    ex = [(i, fx(p), fx(q)) for i, (p, q) in enumerate(zip(A['Q'][k], B['Q'][k])) if p != q][:4]
                    print(f'     pool country #{k[0]} stratum {k[1]}: differs at {ex}')
        print(f'  pools (country x stratum): {n} of {len(keys)} differ')
        # states
        n = tot = 0
        for i in A['S']:
            if i in B['S']:
                for j, (p, q) in enumerate(zip(A['S'][i], B['S'][i])):
                    tot += 1
                    if p != q:
                        n += 1
                        if n <= 6:
                            print(f'     state savings country #{i} state {j}: {fx(p)} vs {fx(q)}')
        print(f'  state savings: {n} of {tot} differ')
        # buildings
        n = tot = 0
        for i in A['B']:
            if i in B['B']:
                for j, (p, q) in enumerate(zip(A['B'][i], B['B'][i])):
                    tot += 1
                    if p != q:
                        n += 1
                        if n <= 6:
                            print(f'     factory country #{i} #{j}: {p} vs {q}')
        print(f'  factories: {n} of {tot} differ')
        # pops
        by_off = {}
        npop = 0
        for k in A['P']:
            if k not in B['P']:
                continue
            for pa, pb in zip(A['P'][k], B['P'][k]):
                npop += 1
                for off, (x, y) in enumerate(zip(pa, pb)):
                    if x != y:
                        by_off.setdefault(off * 4, []).append((k, x, y))
        print(f'  pops compared: {npop}; DWORD offsets that differ: {len(by_off)}')
        for off in sorted(by_off):
            lst = by_off[off]
            k, x, y = lst[0]
            print(f'     +0x{off:03X}: {len(lst):4d} pops differ, e.g. prov/bucket {k}: {x} vs {y}')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
