#!/usr/bin/env python3
"""Compare game-folder fingerprints between machines.

Usage:  python compare_modhash.py host\\v2dll_files.txt client1\\v2dll_files.txt [client2 ...]

The files are written by the DLL itself (5.67+) on the first daily tick: Logs\\v2dll_files.txt.
(The older modhash.ps1 output has the same file-line format, but its hash is MD5 and cannot be
compared with the DLL's FNV-1a 64 - use one kind per comparison.)

The first file is the reference (the host). For every other machine prints the CPU / OS lines,
the per-directory summary (which directories differ), then the files that differ / are missing /
are extra. settings.txt and v2dll_settings.ini are expected to differ between players and are
only listed at the end.
"""
import sys

EXPECTED = ('settings.txt', 'v2dll_settings.ini')


def load(path):
    info, files, dirs = [], {}, {}
    with open(path, encoding='utf-8', errors='replace') as f:
        for line in f:
            line = line.rstrip('\r\n')
            if line.startswith('#'):
                info.append(line)
                continue
            parts = line.split('\t')
            if parts[0] == '@dir' and len(parts) >= 5:
                dirs[parts[1]] = (parts[2], parts[3], parts[4])
            elif len(parts) == 3:
                files[parts[0].lower()] = (parts[1], parts[2])
    return info, files, dirs


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    ref_path = argv[1]
    ref_info, ref, ref_dirs = load(ref_path)
    print(f'=== reference {ref_path}: {len(ref)} files')
    for l in ref_info:
        print('   ', l)
    for p in argv[2:]:
        info, files, dirs = load(p)
        print(f'\n=== {p}: {len(files)} files')
        for l in info:
            print('   ', l)
        bad_dirs = sorted(k for k in set(ref_dirs) | set(dirs) if ref_dirs.get(k) != dirs.get(k))
        print(f'  directories: {len(set(ref_dirs) | set(dirs))}, differing: {len(bad_dirs)}')
        for k in bad_dirs:
            a, b = ref_dirs.get(k), dirs.get(k)
            print(f'   DIR  {k:30s} ref files/bytes/hash {a}  here {b}')
        missing = sorted(set(ref) - set(files))
        extra = sorted(set(files) - set(ref))
        diff = sorted(k for k in set(ref) & set(files) if ref[k] != files[k])
        exp = [k for k in diff if k in EXPECTED]
        diff = [k for k in diff if k not in EXPECTED]
        print(f'  files identical: {len(set(ref) & set(files)) - len(diff) - len(exp)}; different: {len(diff)}; '
              f'missing here: {len(missing)}; extra here: {len(extra)}')
        for k in diff[:100]:
            print(f'   DIFF  {k}  size {ref[k][0]} vs {files[k][0]}  hash {ref[k][1][:8]} vs {files[k][1][:8]}')
        if len(diff) > 100:
            print(f'   ... and {len(diff) - 100} more different files')
        for k in missing[:50]:
            print(f'   MISSING  {k}')
        if len(missing) > 50:
            print(f'   ... and {len(missing) - 50} more missing')
        for k in extra[:50]:
            print(f'   EXTRA    {k}')
        if len(extra) > 50:
            print(f'   ... and {len(extra) - 50} more extra')
        for k in exp:
            print(f'   (expected to differ) {k}')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
