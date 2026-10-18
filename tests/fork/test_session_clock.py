"""Session timing milestones; elapsed time must never deny project work."""
from datetime import datetime, timedelta, timezone
import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("session_clock", Path(__file__).resolve().parents[2] / "tools/planning/session_clock.py")
clock = importlib.util.module_from_spec(spec)
spec.loader.exec_module(clock)


class SessionClockTests(unittest.TestCase):
    def setUp(self):
        self.now = datetime(2026, 10, 18, tzinfo=timezone.utc)
        self.data = clock._empty_ledger()
        clock.start_session(self.data, "S02", "S02-r1", 120, "Focused native profile decision", self.now)

    def test_two_hour_review_is_advisory_not_a_deadline(self):
        at_estimate = clock.check_session(self.data, "S02", "S02-r1", self.now + timedelta(minutes=120))
        self.assertEqual(at_estimate["state"], "WORK_CONTINUES")
        self.assertFalse(at_estimate["post_session_review_pending"])
        over = clock.check_session(self.data, "S02", "S02-r1", self.now + timedelta(minutes=121))
        self.assertEqual(over["state"], "PAUSE_FOR_INDEPENDENT_REVIEW")
        self.assertTrue(over["independent_review_due"])
        self.assertIsNone(over["hard_deadline"])

    def test_scope_reduction_is_separate_from_timing_review(self):
        over = clock.check_session(self.data, "S02", "S02-r1", self.now + timedelta(minutes=301))
        self.assertEqual(over["state"], "SCOPE_REDUCTION_REQUIRED")
        self.assertTrue(over["scope_reduction_due"])
        self.assertTrue(over["independent_review_due"])

    def test_far_under_estimate_requires_post_session_review_before_next_session(self):
        finished = clock.finish_session(self.data, "S02", "S02-r1", "Completed setup rapidly.",
            "Review timing and scope before next session", self.now + timedelta(minutes=20))
        self.assertEqual(finished["state"], "POST_SESSION_REVIEW_REQUIRED")
        closed = clock.check_session(self.data, "S02", "S02-r1", self.now + timedelta(minutes=90))
        self.assertEqual(closed["state"], "SESSION_CLOSED")
        self.assertEqual(closed["session_elapsed_minutes"], 20.0)
        self.assertEqual(closed["family_wall_elapsed_minutes"], 20.0)
        self.assertTrue(closed["post_session_review_pending"])
        with self.assertRaises(ValueError):
            clock.check_session(self.data, "S02", "S02-r1", self.now + timedelta(minutes=19))
        with self.assertRaises(ValueError):
            clock.start_session(self.data, "S02", "S02-r2", 30, "Next scope", self.now + timedelta(minutes=21))
        clock.record_post_session_review(self.data, "S02", "S02-r1", "independent reviewer",
            "Reused setup caused the fast result; native runtime work remains separate.",
            "Preflight and file inspection took little time; retain a separate estimate for runtime work.",
            self.now + timedelta(minutes=25))
        closed_after_review = clock.check_session(self.data, "S02", "S02-r1", self.now + timedelta(minutes=90))
        self.assertEqual(closed_after_review["state"], "SESSION_CLOSED")
        self.assertFalse(closed_after_review["post_session_review_pending"])
        self.assertEqual(clock._get_session(self.data, "S02", "S02-r1")[1]["next_scope"],
            "Review timing and scope before next session")
        next_session = clock.start_session(self.data, "S02", "S02-r2", 30,
            "Proceed with one scoped native gate", self.now + timedelta(minutes=26))
        self.assertEqual(next_session["estimate_minutes"], 30)

    def test_legacy_deadline_is_preserved_as_history_not_enforced(self):
        legacy = {"schema": 1, "families": {"S02": {
            "started_utc": self.now.isoformat(),
            "deadline_utc": (self.now + timedelta(minutes=120)).isoformat(),
            "authorized_minutes": 120,
            "extension": {"minutes": 120, "authorization": "historical"},
        }}}
        with tempfile.TemporaryDirectory() as root:
            path = Path(root) / "session-ledger.json"
            path.write_text(__import__("json").dumps(legacy), encoding="utf-8")
            migrated = clock.read_ledger(path)
            self.assertEqual(migrated["legacy_schema1"]["families"]["S02"]["deadline_utc"], legacy["families"]["S02"]["deadline_utc"])
            session = clock.start_session(migrated, "S02", "S02-r1", 45, "Reduced follow-up scope", self.now)
            self.assertEqual(session["scope"], "Reduced follow-up scope")

    def test_atomic_utf8_persistence(self):
        with tempfile.TemporaryDirectory() as root:
            path = Path(root) / "session-ledger.json"
            clock.write_ledger(path, self.data)
            resumed = clock.read_ledger(path)
            session = clock._get_session(resumed, "S02", "S02-r1")[1]
            self.assertEqual(session["scope"], "Focused native profile decision")
            self.assertFalse(path.with_name(path.name + ".tmp").exists())


if __name__ == "__main__":
    unittest.main()
