"""Persistent admission guard for cumulative session-family deadlines (stdlib only)."""
import argparse
from datetime import datetime, timedelta, timezone
import json
import math
import os
from pathlib import Path
import re
import sys


def utc_now():
    return datetime.now(timezone.utc)


def read_ledger(path):
    if not path.exists():
        return {"schema": 1, "families": {}}
    data = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(data, dict) or data.get("schema") != 1 or not isinstance(data.get("families"), dict):
        raise ValueError("Invalid ledger; reconcile it, never reset it")
    return data


def write_ledger(path, data):
    encoded = (json.dumps(data, indent=2) + "\n").encode("utf-8")
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_name(path.name + ".tmp")
    with temp.open("xb") as stream:
        stream.write(encoded)
        stream.flush()
        os.fsync(stream.fileno())
    os.replace(temp, path)


def start_family(data, family, minutes, now):
    if family not in {"P00"} | {f"S{i:02}" for i in range(1, 31)}:
        raise ValueError("Use a scheduled top-level family; reserve names cannot open a new clock")
    if family == "S02":
        raise ValueError("S02 is historically exhausted; explicit human extension required")
    if family in data["families"]:
        raise ValueError("Family already exists; resume its original deadline, never restart")
    if not 15 < minutes <= 120:
        raise ValueError("Cap must be more than 15 and at most 120 minutes")
    record = {"started_utc": now.isoformat(), "deadline_utc": (now + timedelta(minutes=minutes)).isoformat(), "authorized_minutes": minutes}
    data["families"][family] = record
    return record


def check_family(data, family, packet, operation_minutes, now):
    if family == "S02":
        return 3, {"state": "BUDGET_EXHAUSTED", "family": family, "packet": packet, "reason": "Historical S02/R02 overrun; no automatic continuation"}
    if family not in data["families"]:
        raise ValueError("Missing family ledger; reconcile recorded start before resuming")
    if packet != family and not packet.startswith("R"):
        raise ValueError("A different top-level session needs its own actual objective and ledger")
    if packet.startswith("R02"):
        raise ValueError("R02 descendants belong to exhausted S02, not a new family")
    if not math.isfinite(operation_minutes) or operation_minutes < 0:
        raise ValueError("Operation estimate must be finite and nonnegative")
    record = data["families"][family]
    start = datetime.fromisoformat(record["started_utc"])
    deadline = datetime.fromisoformat(record["deadline_utc"])
    minutes = record["authorized_minutes"]
    extension = record.get("extension")
    extra = 0
    if extension is not None:
        if family != "P00" or not isinstance(extension, dict):
            raise ValueError("Invalid extension record; reconcile it")
        extra = extension.get("minutes", 0)
        if (not isinstance(extra, int) or extra <= 0 or
                not isinstance(extension.get("authorization"), str) or
                not extension["authorization"].strip()):
            raise ValueError("Extension requires positive minutes and an authorization record")
        if minutes != 120 + extra:
            raise ValueError("Extension total does not match the original 120-minute P00 cap")
    if start.tzinfo is None or deadline.tzinfo is None or not 15 < minutes <= 120 + extra:
        raise ValueError("Invalid clock/cap; reconcile ledger")
    if deadline != start + timedelta(minutes=minutes) or now < start:
        raise ValueError("Inconsistent or reversed clock; stop and reconcile")
    remaining = (deadline - now).total_seconds() / 60
    elapsed = minutes - remaining
    code, state = (3, "BUDGET_EXHAUSTED") if remaining <= 0 else ((2, "WRAP_UP_ONLY") if remaining <= 15 or operation_minutes + 15 > remaining else (0, "WORK_ALLOWED"))
    due = [round(minutes * fraction, 3) for fraction in (0.25, 0.5, 0.75) if elapsed >= minutes * fraction]
    return code, {"state": state, "family": family, "packet": packet, "elapsed_minutes": round(elapsed, 3), "remaining_minutes": round(remaining, 3), "deadline_utc": deadline.isoformat(), "assessments_due_by_elapsed_minutes": due}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ledger", required=True, type=Path)
    commands = parser.add_subparsers(dest="command", required=True)
    start = commands.add_parser("start")
    start.add_argument("--family", required=True)
    start.add_argument("--minutes", required=True, type=int)
    check = commands.add_parser("check")
    check.add_argument("--family", required=True)
    check.add_argument("--packet", required=True)
    check.add_argument("--operation-minutes", required=True, type=float)
    args = parser.parse_args(argv)
    try:
        data = read_ledger(args.ledger)
        if args.command == "start":
            record = start_family(data, args.family, args.minutes, utc_now())
            write_ledger(args.ledger, data)
            print(json.dumps({"state": "STARTED", "family": args.family, **record}))
            return 0
        code, result = check_family(data, args.family, args.packet, args.operation_minutes, utc_now())
        print(json.dumps(result))
        return code
    except (ValueError, OSError, KeyError, TypeError) as error:
        print(json.dumps({"state": "STOP_AND_RECONCILE", "error": str(error)}))
        return 1


if __name__ == "__main__":
    sys.exit(main())
