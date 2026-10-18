# S02 session clock closed-state correction

Session: `S02-20261018-clock-closed-state` (15-minute estimate; 1.036 minutes recorded; S02 family wall span 1,210.884 minutes at close).

## Change

`tools/planning/session_clock.py::check_session` now returns `SESSION_CLOSED` for a record with `ended_utc`, reports the recorded `actual_minutes` (or derives elapsed time from start/end when absent), and calculates family wall span through the recorded end. Closed records no longer show active review or scope-reduction work; `post_session_review_pending` reports required closeout review still outstanding. Active records expose that field as false and retain the existing review/scope behavior. A check whose `now` precedes the recorded end is rejected.

Regression coverage in `tests/fork/test_session_clock.py` checks active state, closed state and stored elapsed values, family wall time, pending then completed closeout review, and reversed time.

## Verification

- `python tests/fork/test_session_clock.py`: **5 tests passed**.
- CLI check of the closed session returned `SESSION_CLOSED`, `session_elapsed_minutes: 1.036`, `family_wall_elapsed_minutes: 1210.884`, and `post_session_review_pending: true` before review recording.
- `git diff --check`: passed (Git reported only its normal LF-to-CRLF working-copy notices).
- The session received exactly one closeout timing review. It found 1.036 minutes plausible for this narrowly localized helper fix and calibrated comparable deterministic state-helper estimates to a few minutes. The review did not create work or change scope.

## S02 status

This corrects planning-clock reporting only. It closes no engine, D3D, performance, or product acceptance gate. S02 remains incomplete; all production profiles remain Rejected/WITHHELD; S03 remains gated; P00 remains skipped by user direction and HPM speed remains NOT MEASURED. No game, debugger, candidate, build, or deployment was run.
