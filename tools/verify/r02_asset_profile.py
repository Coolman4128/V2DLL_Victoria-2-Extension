"""Verify pinned VIEWS static asset identities; never authorizes runtime use.

Reads files only. Complete interface corpora and descriptor hashes are pinned to
reviewed R02 evidence. A matching corpus constrains an on-disk input; it does not
prove which assets a running game loaded, dynamic control creation, native GUI
types/lifetime/phase, helper ABI or complete active installer closure.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path


EXPECTED_EXE = '62d48c204364dd706584777c2e2b3c7ab3c5f1dd0170872554943575d53d6648'
REPOSITORY = Path(__file__).resolve().parents[2]
CUSTOM_KEYS = ('FE_ACADEMIES_BDSM', 'FE_RPROJECTS_BDSM',
               'FE_BUDGET_DIPLO_BDSM', 'hide_colonial_states')
VANILLA = {
    'files': 120, 'gui_files': 59,
    'fingerprint': 'bb56b84dfdbe52d4d45d5979b62204d18b3f9912e98a99330b15480df0b90a36',
}
MODS = {
    'ghm': {
        'directory': 'GHM', 'release': '0.6', 'files': 7, 'gui_files': 4,
        'fingerprint': 'b999aa8177c47e341d4b53b0e1c2956082a9975a77b0a80be3057bf99c8dcb67',
        'descriptor_sha256': '964751159215507664c8b334b5ec69a8d6e42620a8a201cd6d91ac881fe329ab',
    },
    'hpm': {
        'directory': 'HPM', 'release': '0.4.6.3', 'files': 20, 'gui_files': 6,
        'fingerprint': '66a37e0a9d1dc0f36522eaf2b3c07e035ffa94e2fe2da443f5ea13a99b26b747',
        'descriptor_sha256': 'd36600358e028a89497b234617af98f106effd41a003a1a03a6183cbd05967b2',
    },
}


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def linked(path):
    return path.is_symlink() or getattr(path, 'is_junction', lambda: False)()


def corpus_snapshot(root):
    """All regular files, ordinal POSIX paths and lowercase SHA-256 + LF."""
    if os.name == 'nt' and not hasattr(Path, 'is_junction'):
        raise ValueError('Windows junction inspection requires Python 3.12 or later')
    if linked(root) or not root.is_dir():
        raise ValueError('interface corpus missing or linked: ' + str(root))
    rows = []
    counts = {key: 0 for key in CUSTOM_KEYS}
    pending = [root]
    while pending:
        with os.scandir(pending.pop()) as children:
            for child in children:
                path = Path(child.path)
                # Reject links before considering descent; eager rglob/sorting
                # can otherwise traverse a Windows junction or cycle first.
                if linked(path):
                    raise ValueError('linked asset is unsupported: ' + str(path))
                if child.is_dir(follow_symlinks=False):
                    pending.append(path)
                    continue
                if not child.is_file(follow_symlinks=False):
                    raise ValueError('non-regular asset is unsupported: ' + str(path))
                content = path.read_bytes()
                lowered = content.lower()
                for key in CUSTOM_KEYS:
                    counts[key] += lowered.count(key.lower().encode('ascii'))
                rows.append({'path': path.relative_to(root).as_posix(),
                             'bytes': len(content), 'sha256': hashlib.sha256(content).hexdigest()})
    rows.sort(key=lambda row: row['path'])
    fingerprint_input = ''.join(row['path'] + '\t' + row['sha256'] + '\n' for row in rows)
    return {
        'root': str(root), 'files': len(rows),
        'gui_files': sum(row['path'].lower().endswith('.gui') for row in rows),
        'fingerprint': hashlib.sha256(fingerprint_input.encode('utf-8')).hexdigest(),
        'custom_key_occurrences_case_insensitive': counts, 'rows': rows,
    }


def compare_corpus(snapshot, expected):
    failures = []
    for key in ('files', 'gui_files', 'fingerprint'):
        if snapshot[key] != expected[key]:
            failures.append(key + ' differs from pinned corpus')
    if any(snapshot['custom_key_occurrences_case_insensitive'].values()):
        failures.append('custom control key is present')
    return failures


def validate_profile(game_dir, workload):
    if workload not in ('vanilla', *MODS):
        raise ValueError('unknown workload')
    report = {
        'schema': 1, 'scope': 'R02 static VIEWS asset identity only',
        'workload': workload, 'static_assets_validated': False,
        'runtime_profile_status': 'unavailable', 'candidate_load_authorized': False,
        'limitations': [
            'No running process or loaded-asset resolution was inspected.',
            'Dynamic controls and native receiver/descriptor types require separate evidence.',
            'Native ABI, lifetime, exclusive phase and full active startup gate remain independent.',
            'File reads are a non-atomic snapshot; eventual supported loading must revalidate.',
            'This report is not a production profile or installer capability.',
        ],
        'failures': [], 'corpora': [],
    }
    exe = game_dir / 'v2game.exe'
    report['executable_sha256'] = sha256(exe)
    if linked(exe) or report['executable_sha256'] != EXPECTED_EXE:
        report['failures'].append('unrecognized executable identity')
        return report  # Do not inspect unrelated assets after an identity failure.
    vanilla = corpus_snapshot(game_dir / 'interface')
    report['corpora'].append(vanilla)
    report['failures'].extend('vanilla: ' + reason for reason in compare_corpus(vanilla, VANILLA))
    if workload != 'vanilla':
        expected = MODS[workload]
        report['release'] = expected['release']
        descriptor = game_dir / 'mod' / (expected['directory'] + '.mod')
        report['descriptor_sha256'] = sha256(descriptor)
        if linked(descriptor) or report['descriptor_sha256'] != expected['descriptor_sha256']:
            report['failures'].append('mod descriptor differs from pinned release')
        corpus = corpus_snapshot(game_dir / 'mod' / expected['directory'] / 'interface')
        report['corpora'].append(corpus)
        report['failures'].extend(workload + ': ' + reason
                                  for reason in compare_corpus(corpus, expected))
    report['static_assets_validated'] = not report['failures']
    return report


def private_destination(path, game_dir=None):
    destination = path.resolve()
    if destination == REPOSITORY or REPOSITORY in destination.parents:
        raise ValueError('evidence destination must be outside repository')
    if game_dir is not None:
        game_root = game_dir.resolve()
        if destination == game_root or game_root in destination.parents:
            raise ValueError('evidence destination must be outside game directory')
    return destination


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-dir', required=True, type=Path)
    parser.add_argument('--workload', required=True, choices=('vanilla', *MODS))
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    try:
        destination = private_destination(args.output, args.game_dir)
        report = validate_profile(args.game_dir, args.workload)
    except (OSError, ValueError) as error:
        parser.exit(1, 'Static asset verification unavailable: ' + str(error) + '\n')
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    state = 'VALIDATED' if report['static_assets_validated'] else 'UNAVAILABLE'
    print(state + ' static assets; runtime profile unavailable; ' + str(destination))
    return 0 if report['static_assets_validated'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
