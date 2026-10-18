"""Persistent estimate clock with at most one timing-only review per session."""
import argparse
from datetime import datetime, timezone
import json
import math
import os
from pathlib import Path
import re
import sys


def utc_now():
    return datetime.now(timezone.utc)


def _empty_ledger():
    return {"schema": 2, "legacy_schema1": None, "families": {}}


def _migrate_v1(data):
    migrated = _empty_ledger()
    migrated["legacy_schema1"] = data
    for family, old_record in data["families"].items():
        migrated["families"][family] = {
            "original_started_utc": old_record.get("started_utc"),
            "historical_clock": old_record,
            "sessions": [],
        }
    return migrated


def read_ledger(path):
    if not path.exists():
        return _empty_ledger()
    data = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(data, dict):
        raise ValueError("Invalid ledger; preserve and reconcile it")
    if data.get("schema") == 1 and isinstance(data.get("families"), dict):
        return _migrate_v1(data)
    if data.get("schema") != 2 or not isinstance(data.get("families"), dict):
        raise ValueError("Invalid ledger; preserve and reconcile it")
    return data


def write_ledger(path, data):
    encoded = (json.dumps(data, indent=2) + "\n").encode("utf-8")
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_name(path.name + ".tmp")
    with temp.open("wb") as stream:
        stream.write(encoded)
        stream.flush()
        os.fsync(stream.fileno())
    os.replace(temp, path)


def _validate_family(family):
    if family != "P00" and not re.fullmatch(r"S(?:0[1-9]|[12][0-9]|30)", family):
        raise ValueError("Use a scheduled top-level family ID; do not create a renamed clock")


def _get_session(data, family, session_id):
    record = data["families"].get(family)
    if not record:
        raise ValueError("Missing family history; reconcile before starting a session")
    for session in record["sessions"]:
        if session["session_id"] == session_id:
            return record, session
    raise ValueError("Unknown session ID")


def start_session(data, family, session_id, estimate_minutes, scope, now):
    _validate_family(family)
    if not isinstance(session_id, str) or not session_id.strip():
        raise ValueError("A unique session ID is required")
    if not isinstance(estimate_minutes, (int, float)) or not math.isfinite(estimate_minutes) or estimate_minutes <= 0:
        raise ValueError("Estimate must be a positive finite number; it is advisory, not a cap")
    if not isinstance(scope, str) or not scope.strip():
        raise ValueError("Record a concrete session scope")
    record = data["families"].setdefault(family, {
        "original_started_utc": now.isoformat(),
        "historical_clock": None,
        "sessions": [],
    })
    if any(session["session_id"] == session_id for session in record["sessions"]):
        raise ValueError("Session ID already exists; continue its record instead of resetting it")
    if record["sessions"]:
        previous = record["sessions"][-1]
        if previous.get("ended_utc") is None:
            raise ValueError("Finish or review the active session before starting another")
        if not previous.get("next_scope"):
            raise ValueError("Record the next session scope before continuing this family")
        if previous.get("post_session_review_required") and not previous.get("post_session_review"):
            raise ValueError("Complete the required independent post-session review before continuing")
    session = {
        "session_id": session_id,
        "estimate_minutes": float(estimate_minutes),
        "scope": scope.strip(),
        "started_utc": now.isoformat(),
        "ended_utc": None,
        "reviews": [],
        "next_scope": None,
        "post_session_review_required": False,
        "post_session_review": None,
    }
    record["sessions"].append(session)
    return session


def check_session(data, family, session_id, now):
    record, session = _get_session(data, family, session_id)
    start = datetime.fromisoformat(session["started_utc"])
    if start.tzinfo is None or now < start:
        raise ValueError("Invalid or reversed session clock; reconcile it")

    if session.get("ended_utc") is not None:
        ended = datetime.fromisoformat(session["ended_utc"])
        if ended.tzinfo is None or ended < start or now < ended:
            raise ValueError("Invalid or reversed session clock; reconcile it")
        elapsed = session.get("actual_minutes")
        if elapsed is None:
            elapsed = (ended - start).total_seconds() / 60
        family_start = record.get("original_started_utc")
        family_elapsed = None
        if family_start:
            family_elapsed = max(
                0.0,
                (ended - datetime.fromisoformat(family_start)).total_seconds() / 60,
            )
        return {
            "state": "SESSION_CLOSED",
            "family": family,
            "session_id": session_id,
            "estimate_minutes": session["estimate_minutes"],
            "session_elapsed_minutes": round(elapsed, 3),
            "family_wall_elapsed_minutes": round(family_elapsed, 3) if family_elapsed is not None else None,
            "independent_review_due": False,
            "scope_reduction_due": False,
            "post_session_review_pending": bool(
                session.get("post_session_review_required") and not session.get("post_session_review")
            ),
            "hard_deadline": None,
        }

    elapsed = (now - start).total_seconds() / 60
    has_independent_review = bool(session.get("reviews") or session.get("post_session_review"))
    independent_review_due = elapsed > 120 and not has_independent_review
    scope_reduction_due = elapsed >= 2.5 * session["estimate_minutes"]
    if scope_reduction_due:
        state = "SCOPE_REDUCTION_REQUIRED"
    elif independent_review_due:
        state = "PAUSE_FOR_INDEPENDENT_REVIEW"
    else:
        state = "WORK_CONTINUES"
    family_start = record.get("original_started_utc")
    family_elapsed = None
    if family_start:
        family_elapsed = max(0.0, (now - datetime.fromisoformat(family_start)).total_seconds() / 60)
    return {
        "state": state,
        "family": family,
        "session_id": session_id,
        "estimate_minutes": session["estimate_minutes"],
        "session_elapsed_minutes": round(elapsed, 3),
        "family_wall_elapsed_minutes": round(family_elapsed, 3) if family_elapsed is not None else None,
        "independent_review_due": independent_review_due,
        "scope_reduction_due": scope_reduction_due,
        "post_session_review_pending": False,
        "hard_deadline": None,
    }


def record_review(data, family, session_id, reviewer, summary, timing_lesson, now):
    _, session = _get_session(data, family, session_id)
    if session.get("ended_utc") is not None:
        raise ValueError("Use post-session review for a closed session")
    if not all(isinstance(value, str) and value.strip() for value in (reviewer, summary, timing_lesson)):
        raise ValueError("Reviewer, timing findings, and calibration lesson are required")
    if session.get("reviews") or session.get("post_session_review"):
        raise ValueError("This session already received its one independent review")
    elapsed = (now - datetime.fromisoformat(session["started_utc"])).total_seconds() / 60
    if elapsed <= 120:
        raise ValueError("The independent in-session review is due only after two hours")
    review = {
        "kind": "overrun-checkpoint",
        "reviewer": reviewer.strip(),
        "summary": summary.strip(),
        "timing_lesson": timing_lesson.strip(),
        "reviewed_utc": now.isoformat(),
        "elapsed_minutes": round(elapsed, 3),
    }
    session["reviews"].append(review)
    session["checkpoint_reviewed_through_minutes"] = round(elapsed, 3)
    return review


def finish_session(data, family, session_id, retrospective, next_scope, now):
    _, session = _get_session(data, family, session_id)
    if session.get("ended_utc") is not None:
        raise ValueError("Session is already closed")
    if not isinstance(retrospective, str) or not retrospective.strip():
        raise ValueError("Record the session retrospective")
    if not isinstance(next_scope, str) or not next_scope.strip():
        raise ValueError("Update the next session scope before closing")
    start = datetime.fromisoformat(session["started_utc"])
    elapsed = (now - start).total_seconds() / 60
    if elapsed < 0:
        raise ValueError("Session clock moved backward")
    under_half = elapsed < 0.5 * session["estimate_minutes"]
    over_two_hours = elapsed > 120
    already_reviewed = bool(session.get("reviews") or session.get("post_session_review"))
    session["ended_utc"] = now.isoformat()
    session["actual_minutes"] = round(elapsed, 3)
    session["retrospective"] = retrospective.strip()
    session["next_scope"] = next_scope.strip()
    session["post_session_review_required"] = not already_reviewed and (under_half or over_two_hours)
    return {
        "state": "POST_SESSION_REVIEW_REQUIRED" if session["post_session_review_required"] else "SESSION_RECORDED",
        "family": family,
        "session_id": session_id,
        "estimate_minutes": session["estimate_minutes"],
        "actual_minutes": round(elapsed, 3),
        "far_under_estimate": under_half,
        "over_two_hours": over_two_hours,
        "hard_deadline": None,
    }


def record_post_session_review(data, family, session_id, reviewer, summary, timing_lesson, now):
    _, session = _get_session(data, family, session_id)
    if session.get("ended_utc") is None:
        raise ValueError("Close the session before post-session review")
    if not session.get("post_session_review_required"):
        raise ValueError("This session does not require an independent timing review")
    if session.get("post_session_review") or session.get("reviews"):
        raise ValueError("This session already received its one independent review")
    if not all(isinstance(value, str) and value.strip() for value in (reviewer, summary, timing_lesson)):
        raise ValueError("Reviewer, timing findings, and calibration lesson are required")
    session["post_session_review"] = {
        "reviewer": reviewer.strip(),
        "summary": summary.strip(),
        "timing_lesson": timing_lesson.strip(),
        "reviewed_utc": now.isoformat(),
    }
    return session["post_session_review"]


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ledger", required=True, type=Path)
    commands = parser.add_subparsers(dest="command", required=True)
    start = commands.add_parser("start")
    start.add_argument("--family", required=True)
    start.add_argument("--session-id", required=True)
    start.add_argument("--estimate-minutes", required=True, type=float)
    start.add_argument("--scope", required=True)
    for name in ("check",):
        command = commands.add_parser(name)
        command.add_argument("--family", required=True)
        command.add_argument("--session-id", required=True)
    review = commands.add_parser("review")
    review.add_argument("--family", required=True)
    review.add_argument("--session-id", required=True)
    review.add_argument("--reviewer", required=True)
    review.add_argument("--summary", required=True)
    review.add_argument("--timing-lesson", required=True)
    finish = commands.add_parser("finish")
    finish.add_argument("--family", required=True)
    finish.add_argument("--session-id", required=True)
    finish.add_argument("--retrospective", required=True)
    finish.add_argument("--next-scope", required=True)
    post = commands.add_parser("post-review")
    post.add_argument("--family", required=True)
    post.add_argument("--session-id", required=True)
    post.add_argument("--reviewer", required=True)
    post.add_argument("--summary", required=True)
    post.add_argument("--timing-lesson", required=True)
    args = parser.parse_args(argv)
    try:
        data = read_ledger(args.ledger)
        now = utc_now()
        if args.command == "start":
            result = start_session(data, args.family, args.session_id, args.estimate_minutes, args.scope, now)
            output = {"state": "SESSION_STARTED", "family": args.family, **result}
        elif args.command == "check":
            output = check_session(data, args.family, args.session_id, now)
        elif args.command == "review":
            output = {"state": "REVIEW_RECORDED", **record_review(
                data, args.family, args.session_id, args.reviewer,
                args.summary, args.timing_lesson, now)}
        elif args.command == "finish":
            output = finish_session(data, args.family, args.session_id, args.retrospective, args.next_scope, now)
        else:
            output = {"state": "POST_SESSION_REVIEW_RECORDED", **record_post_session_review(
                data, args.family, args.session_id, args.reviewer, args.summary, args.timing_lesson, now)}
        write_ledger(args.ledger, data)
        print(json.dumps(output))
        return 0
    except (ValueError, OSError, KeyError, TypeError) as error:
        print(json.dumps({"state": "RECONCILE_SESSION_RECORD", "error": str(error)}))
        return 1


if __name__ == "__main__":
    sys.exit(main())
