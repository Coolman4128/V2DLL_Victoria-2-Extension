"""Meaningful deadline, continuation and restart contracts; no game execution."""
from datetime import datetime, timedelta, timezone
import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("budget_guard", Path(__file__).resolve().parents[2] / "tools/planning/budget_guard.py")
guard = importlib.util.module_from_spec(spec)
spec.loader.exec_module(guard)


class BudgetTests(unittest.TestCase):
    def setUp(self):
        self.now = datetime(2026, 10, 18, tzinfo=timezone.utc)
        self.data = {"schema": 1, "families": {}}
        guard.start_family(self.data, "S05", 120, self.now)

    def check(self, minutes, operation=1, packet="S05"):
        return guard.check_family(self.data, "S05", packet, operation, self.now + timedelta(minutes=minutes))

    def test_deadline_boundaries_and_assessments(self):
        self.assertEqual(self.check(29)[1]["assessments_due_by_elapsed_minutes"], [])
        self.assertEqual(self.check(90)[1]["assessments_due_by_elapsed_minutes"], [30, 60, 90])
        self.assertEqual(self.check(104)[0], 0)
        self.assertEqual(self.check(105)[0], 2)
        self.assertEqual(self.check(120)[0], 3)
        self.assertEqual(self.check(240)[0], 3)

    def test_operation_must_fit_before_wrapup(self):
        self.assertEqual(self.check(100, operation=6)[0], 2)
        self.assertEqual(self.check(100, operation=5)[0], 0)

    def test_reserve_inherits_parent_deadline(self):
        self.assertEqual(self.check(121, packet="R08b")[0], 3)
        self.assertEqual(self.check(100, packet="R08b")[1]["deadline_utc"], self.data["families"]["S05"]["deadline_utc"])

    def test_restart_and_rename_cannot_reopen_s02(self):
        with self.assertRaises(ValueError):
            guard.start_family(self.data, "S05", 120, self.now + timedelta(days=1))
        with self.assertRaises(ValueError):
            guard.start_family(self.data, "S02", 120, self.now)
        self.assertEqual(guard.check_family(self.data, "S02", "R02x", 1, self.now)[0], 3)
        with self.assertRaises(ValueError):
            self.check(0, packet="R02x")
        for alias in ("R02", "R02X", "S02NEW", "S31"):
            with self.assertRaises(ValueError):
                guard.start_family(self.data, alias, 120, self.now)

    def test_invalid_estimate_cannot_bypass_reserve(self):
        for estimate in (float("nan"), float("inf"), -1):
            with self.assertRaises(ValueError):
                self.check(100, operation=estimate)

    def test_invalid_or_missing_clock_fails_closed(self):
        with self.assertRaises(ValueError):
            guard.check_family(self.data, "S06", "S06", 1, self.now)
        self.data["families"]["S05"]["deadline_utc"] = (self.now + timedelta(days=1)).isoformat()
        with self.assertRaises(ValueError):
            self.check(0)

    def test_atomic_utf8_persistence_across_resume(self):
        with tempfile.TemporaryDirectory() as root:
            path = Path(root) / "ledger.json"
            guard.write_ledger(path, self.data)
            resumed = guard.read_ledger(path)
            self.assertEqual(guard.check_family(resumed, "S05", "R08c", 1, self.now + timedelta(minutes=121))[0], 3)
            self.assertFalse(path.with_name(path.name + ".tmp").exists())


if __name__ == "__main__":
    unittest.main()
