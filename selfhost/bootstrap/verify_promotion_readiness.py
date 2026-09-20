#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--contract", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--nsc0", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    try:
        contract = json.loads(args.contract.read_text(encoding="utf-8"))
        if contract.get("schema") != "nativesharp-selfhost-promotion-readiness-contract":
            raise ValueError("bad promotion contract")
        args.output_dir.mkdir(parents=True, exist_ok=True)
        gates: dict[str, dict] = {}
        blockers: list[str] = []
        for name in contract["required_landed_gates"]:
            gate_path = args.source_root / "selfhost/evidence/gates" / name / "gate.json"
            gate = json.loads(gate_path.read_text(encoding="utf-8")); gates[name] = gate
            if gate.get("state") != "LANDED": blockers.append(f"{name} state is {gate.get('state')}, not LANDED")
            blockers.extend(f"{name}: {problem}" for problem in gate.get("problems", []))
        source_derived = bool(gates.get("SH13", {}).get("metrics", {}).get("source_derived_codegen", False))
        if not source_derived: blockers.append("source-derived compiler codegen is not certified")
        promoted = not blockers
        decision = {
            "schema": "nativesharp-selfhost-promotion-decision", "version": 1,
            "decision": "promote" if promoted else "withhold",
            "default_compiler": contract["candidate_compiler"] if promoted else contract["fallback_compiler"],
            "candidate_compiler": contract["candidate_compiler"],
            "rollback_compiler": contract["fallback_compiler"],
            "nsc0_sha256": hashlib.sha256(args.nsc0.read_bytes()).hexdigest(),
            "gate_states": {name: gate.get("state") for name, gate in gates.items()},
            "source_derived_codegen": source_derived, "blockers": blockers,
            "work_items": contract["work_items"],
        }
        args.report.write_text(json.dumps(decision, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        (args.output_dir / "compiler-selection.json").write_text(json.dumps({
            "schema": "nativesharp-compiler-selection", "version": 1,
            "default": decision["default_compiler"], "candidate": decision["candidate_compiler"],
            "rollback": decision["rollback_compiler"], "promotion_allowed": promoted,
        }, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        if promoted:
            print("SH14 promotion requirements satisfied")
        else:
            print(f"SH14 promotion safely withheld: {len(blockers)} blockers")
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"verify_promotion_readiness: error: {exc}", file=sys.stderr); return 1
    return 0


if __name__ == "__main__": raise SystemExit(main())
