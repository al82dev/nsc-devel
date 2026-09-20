#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--contract", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    try:
        contract = json.loads(args.contract.read_text(encoding="utf-8"))
        if contract.get("schema") != "nativesharp-selfhost-closure-contract":
            raise ValueError("bad closure contract")
        if contract.get("tandas") != list(range(7, 17)):
            raise ValueError("closure contract must cover tandas 7 through 16")
        expected_states = contract.get("expected_gate_states", {})
        if set(expected_states) != set(contract["required_gates"]):
            raise ValueError("expected gate states do not match required gates")
        expected_commit_keys = {str(number) for number in range(7, 16)}
        known_commits = contract.get("known_commits", {})
        if set(known_commits) != expected_commit_keys:
            raise ValueError("known commits must cover tandas 7 through 15")
        if any(not re.fullmatch(r"[0-9a-f]{9}", value) for value in known_commits.values()):
            raise ValueError("known commits must use nine lowercase hexadecimal digits")
        args.output_dir.mkdir(parents=True, exist_ok=True)
        gates: dict[str, dict] = {}
        hashes: dict[str, str] = {}
        for gate_name in contract["required_gates"]:
            path = args.source_root / "selfhost/evidence/gates" / gate_name / "gate.json"
            gate = json.loads(path.read_text(encoding="utf-8"))
            if gate.get("gate") != gate_name:
                raise ValueError(f"gate identity mismatch for {gate_name}")
            if gate.get("state") != expected_states[gate_name]:
                raise ValueError(
                    f"gate state changed for {gate_name}: "
                    f"expected {expected_states[gate_name]}, got {gate.get('state')}"
                )
            gates[gate_name] = gate
            hashes[gate_name] = sha256(path)
        selection_path = args.source_root / "selfhost/evidence/gates/SH14/compiler-selection.json"
        selection = json.loads(selection_path.read_text(encoding="utf-8"))
        if selection.get("default") != contract["required_safe_default"]:
            raise ValueError("unsafe default compiler")
        if selection.get("promotion_allowed") is not contract["required_promotion_allowed"]:
            raise ValueError("promotion state changed")
        unresolved = [
            {
                "id": "SELFHOST-CROSS-MACHINE",
                "condition": "Import matching SH12 machine-attestation.json from a second Windows x64 machine",
                "owner_files": ["selfhost/bootstrap/verify_fixed_point.py", "selfhost/evidence/gates/SH12/gate.json"],
                "blocks": ["SH12", "SH13", "SH14"],
            },
            {
                "id": "SELFHOST-SOURCE-DERIVED-CODEGEN",
                "condition": "Replace source-attested image replay with complete source-derived LLVM code generation",
                "owner_files": ["selfhost/nsc/CodeGen/LlvmBackend.ns", "selfhost/nsc/Bootstrap/GenerationBuilder.ns"],
                "blocks": ["SH13", "SH14", "nsc0 retirement"],
            },
        ]
        report = {
            "schema": "nativesharp-selfhost-tanda16-closure",
            "version": 1,
            "status": "closed-with-explicit-blockers",
            "tandas": contract["tandas"],
            "known_commits": contract["known_commits"],
            "gate_states": {name: gates[name]["state"] for name in contract["required_gates"]},
            "gate_sha256": hashes,
            "safe_selection": selection,
            "release_promoted": False,
            "nsc0_retired": False,
            "unresolved": unresolved,
            "closure_problems": [],
        }
        args.report.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        (args.output_dir / "remaining-work.json").write_text(
            json.dumps(
                {
                    "schema": "nativesharp-selfhost-remaining-work",
                    "version": 1,
                    "items": unresolved,
                },
                indent=2,
                sort_keys=True,
            )
            + "\n",
            encoding="utf-8",
        )
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"verify_closure: error: {exc}", file=sys.stderr)
        return 1
    print("Tanda 16 closure sealed: safe default retained and two blockers made executable")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
