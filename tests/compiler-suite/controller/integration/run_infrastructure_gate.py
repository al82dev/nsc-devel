from __future__ import annotations

import argparse
import sys
import tempfile
from pathlib import Path
from typing import Any


CONTROLLER_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(CONTROLLER_ROOT))

from run_suite import run
from component_gate_support import assert_component_results, read_json_object, read_jsonl
from nsc_suite import CaseManifest, InventoryIndex, Selection, load_manifest, select_cases


def fixture_cases(fixture: Path) -> tuple[CaseManifest, ...]:
    inventory = InventoryIndex.from_csv(
        fixture / "components.csv",
        fixture / "contracts.csv",
    )
    cases = load_manifest(fixture / "cases.jsonl", inventory)
    return select_cases(cases, Selection(all_cases=True, build_mode="Debug"))


def validate_results(
    cases: tuple[CaseManifest, ...], payload: dict[str, Any]
) -> None:
    assert_component_results(
        payload,
        cases,
        "Infrastructure fixture",
        selection_name="live fixture selection",
    )


def validate_events(
    cases: tuple[CaseManifest, ...], events: list[dict[str, Any]]
) -> None:
    expected_ids = sorted(case.case_id for case in cases)
    if len(events) != 2 + (2 * len(cases)):
        raise RuntimeError("controller event stream has invalid protocol cardinality")
    if any(not isinstance(event, dict) for event in events):
        raise RuntimeError("controller event stream contains a non-object event")
    sequences = [event.get("sequence") for event in events]
    if any(type(sequence) is not int for sequence in sequences):
        raise RuntimeError("controller event sequence must contain exact integers")
    if sequences != list(range(len(events))):
        raise RuntimeError("controller event sequence is not contiguous")
    if events[0].get("kind") != "RUN_STARTED":
        raise RuntimeError("controller event stream does not start with RUN_STARTED")
    if events[-1].get("kind") != "RUN_FINISHED":
        raise RuntimeError("controller event stream does not end with RUN_FINISHED")
    started_ids = [
        event.get("case_id")
        for event in events
        if event.get("kind") == "CASE_STARTED"
    ]
    finished_ids = [
        event.get("case_id")
        for event in events
        if event.get("kind") == "CASE_FINISHED"
    ]
    if any(not isinstance(case_id, str) or not case_id for case_id in started_ids + finished_ids):
        raise RuntimeError("controller event stream contains an invalid case identity")
    started = sorted(started_ids)
    finished = sorted(finished_ids)
    if started != expected_ids or finished != expected_ids:
        raise RuntimeError("controller events do not cover the live fixture selection")
    expected_statuses = {case.case_id: case.expected_outcome for case in cases}
    for event in events:
        if event.get("kind") != "CASE_FINISHED":
            continue
        payload = event.get("payload")
        if not isinstance(payload, dict):
            raise RuntimeError("CASE_FINISHED payload is not an object")
        status = payload.get("status")
        if not isinstance(status, str) or not status:
            raise RuntimeError("CASE_FINISHED contains an invalid status")
        case_id = event["case_id"]
        if status != expected_statuses[case_id]:
            raise RuntimeError(
                f"CASE_FINISHED status differs from declared outcome for {case_id}"
            )
    start_payload = events[0].get("payload", {})
    finish_payload = events[-1].get("payload", {})
    if not isinstance(start_payload, dict) or not isinstance(finish_payload, dict):
        raise RuntimeError("controller terminal event payload is not an object")
    selected = start_payload.get("selected")
    if type(selected) is not int or selected != len(cases):
        raise RuntimeError("RUN_STARTED selected count differs from the fixture")
    finish_selected = finish_payload.get("selected")
    completed = finish_payload.get("completed")
    if (
        type(finish_selected) is not int
        or finish_selected != len(cases)
        or type(completed) is not int
        or completed != len(cases)
        or finish_payload.get("cancelled") is not False
    ):
        raise RuntimeError("RUN_FINISHED payload does not close the fixture run")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host-executable", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--work-parent", type=Path, required=True)
    options = parser.parse_args()
    options.work_parent.mkdir(parents=True, exist_ok=True)
    fixture = options.source_root / "fixtures" / "infrastructure"
    cases = fixture_cases(fixture)
    with tempfile.TemporaryDirectory(
        prefix="controller-gate-", dir=options.work_parent
    ) as temporary:
        root = Path(temporary)
        code = run(
            [
                "--manifest", str(fixture / "cases.jsonl"),
                "--components", str(fixture / "components.csv"),
                "--contracts", str(fixture / "contracts.csv"),
                "--host-executable", str(options.host_executable),
                "--work-root", str(root),
                "--run-id", "infrastructure",
                "--revision", "synthetic-infrastructure-gate",
                "--build-mode", "Debug",
                "--all",
            ]
        )
        if code != 0:
            raise RuntimeError(f"controller returned {code}")
        run_root = root / "infrastructure"
        payload = read_json_object(run_root / "results.json")
        validate_results(cases, payload)
        events = read_jsonl(run_root / "events.jsonl")
        validate_events(cases, events)
        for required in ("summary.md", "failures.log"):
            if not run_root.joinpath(required).is_file():
                raise RuntimeError(f"controller omitted {required}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
