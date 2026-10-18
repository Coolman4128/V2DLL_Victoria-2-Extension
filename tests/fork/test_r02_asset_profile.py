"""Synthetic contracts for the R02 static asset profile verifier."""
import hashlib
import importlib.util
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest import mock


REPOSITORY = Path(__file__).resolve().parents[2]
TOOL = REPOSITORY / 'tools' / 'verify' / 'r02_asset_profile.py'
SPEC = importlib.util.spec_from_file_location('r02_asset_profile', TOOL)
PROFILE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PROFILE)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)


def expected_from(root):
    snapshot = PROFILE.corpus_snapshot(root)
    return {key: snapshot[key] for key in ('files', 'gui_files', 'fingerprint')}


class AssetProfileContracts(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)

    def tearDown(self):
        self.temp.cleanup()

    def make_game(self, interface_files):
        game = self.root / 'game'
        exe_bytes = b'synthetic executable fixture'
        write(game / 'v2game.exe', exe_bytes)
        interface = game / 'interface'
        for rel, content in interface_files:
            write(interface / rel, content)
        return game, interface, exe_bytes

    def test_fingerprint_hashes_sorted_nested_posix_paths(self):
        _, interface, _ = self.make_game([
            ('z.gui', b'zeta'),
            ('nested/b.txt', b'beta'),
            ('a.gui', b'alpha'),
            ('nested/a.gui', b'nested'),
        ])
        snapshot = PROFILE.corpus_snapshot(interface)
        rows = sorted([
            ('z.gui', b'zeta'), ('nested/b.txt', b'beta'),
            ('a.gui', b'alpha'), ('nested/a.gui', b'nested'),
        ])
        canonical = ''.join(
            f'{rel}\t{digest(content)}\n' for rel, content in rows
        )
        self.assertEqual(snapshot['fingerprint'], digest(canonical.encode('utf-8')))
        self.assertEqual([row['path'] for row in snapshot['rows']],
                         [rel for rel, _ in rows])
        self.assertEqual(snapshot['gui_files'], 3)

    def test_reordered_creation_has_same_fingerprint(self):
        files = [('sub/c.gui', b'c'), ('b.txt', b'b'), ('a.gui', b'a')]
        first, first_interface, _ = self.make_game(files)
        second = self.root / 'second'
        for rel, content in reversed(files):
            write(second / 'interface' / rel, content)
        self.assertEqual(PROFILE.corpus_snapshot(first_interface)['fingerprint'],
                         PROFILE.corpus_snapshot(second / 'interface')['fingerprint'])

    def test_missing_extra_and_changed_assets_fail_captured_corpus(self):
        _, interface, _ = self.make_game([('one.gui', b'one'), ('nested/two.txt', b'two')])
        expected = expected_from(interface)
        baseline = PROFILE.corpus_snapshot(interface)
        self.assertEqual(PROFILE.compare_corpus(baseline, expected), [])

        (interface / 'nested' / 'two.txt').unlink()
        self.assertIn('files differs from pinned corpus',
                      PROFILE.compare_corpus(PROFILE.corpus_snapshot(interface), expected))
        write(interface / 'nested' / 'two.txt', b'two')

        write(interface / 'extra.gui', b'extra')
        self.assertIn('files differs from pinned corpus',
                      PROFILE.compare_corpus(PROFILE.corpus_snapshot(interface), expected))
        (interface / 'extra.gui').unlink()

        write(interface / 'nested' / 'two.txt', b'changed')
        self.assertIn('fingerprint differs from pinned corpus',
                      PROFILE.compare_corpus(PROFILE.corpus_snapshot(interface), expected))

    def test_custom_keys_are_detected_case_insensitively(self):
        _, interface, _ = self.make_game([('controls.GUI', b'layout FE_academies_bdsm\n')])
        snapshot = PROFILE.corpus_snapshot(interface)
        self.assertEqual(snapshot['custom_key_occurrences_case_insensitive']['FE_ACADEMIES_BDSM'], 1)
        self.assertIn('custom control key is present',
                      PROFILE.compare_corpus(snapshot, expected_from(interface)))

    def test_wrong_executable_identity_stops_before_asset_reads(self):
        game, _, exe_bytes = self.make_game([('a.gui', b'a')])
        with mock.patch.object(PROFILE, 'EXPECTED_EXE', digest(exe_bytes + b'expected')):
            with mock.patch.object(PROFILE, 'corpus_snapshot') as snapshot:
                report = PROFILE.validate_profile(game, 'vanilla')
        snapshot.assert_not_called()
        self.assertEqual(report['failures'], ['unrecognized executable identity'])
        self.assertFalse(report['static_assets_validated'])

    def test_descriptor_mismatch_is_reported_with_synthetic_mod_constants(self):
        game, interface, exe_bytes = self.make_game([('base.gui', b'base')])
        expected_vanilla = expected_from(interface)
        mod_dir = game / 'mod' / 'fixture'
        mod_interface = mod_dir / 'interface'
        write(mod_interface / 'budget.gui', b'fixture budget')
        descriptor = game / 'mod' / 'fixture.mod'
        write(descriptor, b'name = "fixture"')
        expected_mod = {
            'directory': 'fixture', 'release': 'synthetic',
            **expected_from(mod_interface),
            'descriptor_sha256': digest(b'different descriptor'),
        }
        with mock.patch.object(PROFILE, 'EXPECTED_EXE', digest(exe_bytes)), \
             mock.patch.object(PROFILE, 'VANILLA', expected_vanilla), \
             mock.patch.dict(PROFILE.MODS, {'fixture': expected_mod}):
            report = PROFILE.validate_profile(game, 'fixture')
        self.assertIn('mod descriptor differs from pinned release', report['failures'])
        self.assertFalse(report['static_assets_validated'])

    def test_missing_corpus_and_descriptor_fail_safely(self):
        game = self.root / 'missing-game'
        exe_bytes = b'executable'
        write(game / 'v2game.exe', exe_bytes)
        with mock.patch.object(PROFILE, 'EXPECTED_EXE', digest(exe_bytes)), \
             mock.patch.object(PROFILE, 'VANILLA', {'files': 0, 'gui_files': 0, 'fingerprint': digest(b'')}):
            with self.assertRaises(ValueError):
                PROFILE.validate_profile(game, 'vanilla')

        interface = game / 'interface'
        write(interface / 'a.gui', b'a')
        vanilla_expected = expected_from(interface)
        mod_expected = {
            'directory': 'fixture', 'release': 'synthetic',
            'files': 0, 'gui_files': 0, 'fingerprint': digest(b''),
            'descriptor_sha256': digest(b'descriptor'),
        }
        with mock.patch.object(PROFILE, 'EXPECTED_EXE', digest(exe_bytes)), \
             mock.patch.object(PROFILE, 'VANILLA', vanilla_expected), \
             mock.patch.dict(PROFILE.MODS, {'fixture': mod_expected}):
            with self.assertRaises(FileNotFoundError):
                PROFILE.validate_profile(game, 'fixture')

    def test_unknown_workload_fails_before_filesystem_access(self):
        with mock.patch.object(PROFILE, 'sha256') as file_hash:
            with self.assertRaisesRegex(ValueError, 'unknown workload'):
                PROFILE.validate_profile(self.root / 'absent', 'unknown')
        file_hash.assert_not_called()

    def test_repository_evidence_destination_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'outside repository'):
            PROFILE.private_destination(REPOSITORY / 'private-evidence.json')

    def test_game_tree_evidence_destination_is_rejected(self):
        game = self.root / 'game-tree'
        output = game / 'logs' / 'asset-profile.json'
        with self.assertRaisesRegex(ValueError, 'outside game directory'):
            PROFILE.private_destination(output, game)

    def test_linked_asset_rejected_when_symlinks_are_available(self):
        _, interface, _ = self.make_game([('base.gui', b'base')])
        external = self.root / 'external.txt'
        write(external, b'external')
        link = interface / 'linked.txt'
        try:
            link.symlink_to(external)
        except (OSError, NotImplementedError) as error:
            self.skipTest('symlink creation unavailable: ' + str(error))
        with self.assertRaisesRegex(ValueError, 'linked asset is unsupported'):
            PROFILE.corpus_snapshot(interface)

    def test_directory_junction_cycle_is_rejected_before_descent(self):
        powershell = shutil.which('powershell.exe') or shutil.which('powershell')
        if not powershell:
            self.skipTest('Windows PowerShell unavailable for junction fixture')

        game, interface, _ = self.make_game([('base.gui', b'base')])
        target_marker = game / 'target-survival.txt'
        write(target_marker, b'must survive junction cleanup')
        junction = interface / 'cycle'
        target = game
        owned_root = self.root.resolve()
        link_abs = Path(os.path.abspath(junction))
        target_abs = target.resolve()
        self.assertEqual(os.path.commonpath((str(owned_root), str(link_abs))), str(owned_root))
        self.assertEqual(os.path.commonpath((str(owned_root), str(target_abs))), str(owned_root))

        env = os.environ.copy()
        env['V2DLL_TEST_JUNCTION_LINK'] = str(link_abs)
        env['V2DLL_TEST_JUNCTION_TARGET'] = str(target_abs)
        create = subprocess.run(
            [powershell, '-NoProfile', '-NonInteractive', '-Command',
             "New-Item -ItemType Junction -Path $env:V2DLL_TEST_JUNCTION_LINK "
             "-Target $env:V2DLL_TEST_JUNCTION_TARGET | Out-Null"],
            env=env, capture_output=True, text=True, timeout=20,
        )
        if create.returncode:
            message = (create.stdout + create.stderr).strip()
            low = message.lower()
            if any(token in low for token in ('privilege', 'access is denied', 'access denied',
                                               'not held by the client', 'administrator')):
                self.skipTest('junction creation unavailable: ' + message)
            self.fail('PowerShell junction creation failed: ' + message)

        try:
            self.assertTrue(PROFILE.linked(junction), 'fixture must be detected as a junction')
            with self.assertRaisesRegex(ValueError, 'linked asset is unsupported'):
                PROFILE.corpus_snapshot(interface)
        finally:
            # os.rmdir unlinks this directory junction entry without following it.
            # Keep the link path lexical; resolving it would name the target instead.
            self.assertEqual(os.path.commonpath((str(owned_root), str(link_abs))), str(owned_root))
            self.assertEqual(os.path.commonpath((str(owned_root), str(target.resolve()))),
                             str(owned_root))
            self.assertTrue(target_marker.is_file(), 'target marker must exist before unlink')
            os.rmdir(link_abs)
            self.assertFalse(junction.exists())
            self.assertEqual(target_marker.read_bytes(), b'must survive junction cleanup')

    def test_matching_static_fixture_never_authorizes_runtime_or_candidate(self):
        game, interface, exe_bytes = self.make_game([('plain.gui', b'plain layout')])
        expected = expected_from(interface)
        with mock.patch.object(PROFILE, 'EXPECTED_EXE', digest(exe_bytes)), \
             mock.patch.object(PROFILE, 'VANILLA', expected):
            report = PROFILE.validate_profile(game, 'vanilla')
        self.assertTrue(report['static_assets_validated'])
        self.assertEqual(report['runtime_profile_status'], 'unavailable')
        self.assertFalse(report['candidate_load_authorized'])


if __name__ == '__main__':
    unittest.main()
