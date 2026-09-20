#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path


class VerificationError(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise VerificationError(message)


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description="Verify retirement of bootstrap image replay.")
    parser.add_argument("--contract", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--nsc1", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    arguments = parser.parse_args()
    try:
        contract = json.loads(arguments.contract.read_text(encoding="utf-8"))
        require(contract.get("schema") == "nativesharp-selfhost-image-replay-retirement",
                "unsupported image-replay retirement contract")
        require(contract.get("image_replay_allowed") is False, "image replay must be forbidden")
        arguments.output_dir.mkdir(parents=True, exist_ok=True)

        builder_path = arguments.source_root / "selfhost/nsc/Bootstrap/GenerationBuilder.ns"
        builder = builder_path.read_text(encoding="utf-8")
        for fragment in contract["prohibited_source_fragments"]:
            require(fragment not in builder, f"prohibited replay fragment remains: {fragment}")

        sources = sorted((arguments.source_root / "selfhost/nsc").rglob("*.ns"))
        require(len(sources) >= contract["minimum_compiler_sources"], "compiler source set is incomplete")
        manifest = arguments.output_dir / "compiler-sources.manifest"
        manifest.write_text("".join(f"{path}\n" for path in sources), encoding="utf-8")
        output = arguments.output_dir / "forbidden-nsc2.exe"
        trace = arguments.output_dir / "nsc2-blocked.trace"
        output.unlink(missing_ok=True)
        trace.unlink(missing_ok=True)

        producer_before = digest(arguments.nsc1)
        completed = subprocess.run(
            [str(arguments.nsc1), "--bootstrap-generation", contract["generation"],
             str(manifest), str(arguments.nsc1), str(output), str(trace)],
            text=True, encoding="utf-8", errors="strict", stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, check=False,
        )
        require(completed.returncode == contract["blocked_exit_code"],
                f"generation did not fail closed: {completed.returncode}")
        require(not output.exists(), "blocked generation materialized an executable")
        require(trace.is_file(), "blocked generation did not write its audit trace")
        require(digest(arguments.nsc1) == producer_before, "producer image changed")
        fields = dict(
            line.split("=", 1) for line in trace.read_text(encoding="utf-8").splitlines()
            if "=" in line
        )
        for key, value in contract["required_trace"].items():
            require(fields.get(key) == value, f"trace field {key} changed")
        require(int(fields.get("sources", "0")) == len(sources), "trace source count changed")

        report = {
            "schema": "nativesharp-selfhost-sh15-report",
            "version": 1,
            "status": "passed",
            "gate": "SH15",
            "image_replay_retired": True,
            "source_derived_codegen": False,
            "generation_blocked": True,
            "blocked_exit_code": completed.returncode,
            "compiler_sources": len(sources),
            "producer_sha256": producer_before,
            "trace": fields,
            "stdout": completed.stdout,
            "stderr": completed.stderr,
            "problems": [],
        }
        arguments.report.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    except (OSError, UnicodeError, ValueError, json.JSONDecodeError, VerificationError) as error:
        print(f"verify_image_replay_retired: error: {error}", file=sys.stderr)
        return 1
    print("SH15 image replay retired: generation fails closed until source-derived codegen exists")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
