from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path
from typing import Sequence


SCHEMA = "nativesharp-selfhost-semantic-contract"
REPORT_SCHEMA = "nativesharp-selfhost-sh06-report"
VERSION = 1


class VerificationError(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise VerificationError(message)


def run(executable: Path, action: str, source: Path) -> subprocess.CompletedProcess[bytes]:
    return subprocess.run(
        [str(executable), action, str(source)], stdout=subprocess.PIPE,
        stderr=subprocess.PIPE, check=False, timeout=120,
    )


def canonical_lines(data: bytes) -> bytes:
    return data.replace(b"\r\n", b"\n").replace(b"\r", b"\n")


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_contract(path: Path) -> dict:
    contract = json.loads(path.read_text(encoding="utf-8"))
    require(contract.get("schema") == SCHEMA, "unexpected semantic contract schema")
    require(contract.get("version") == VERSION, "unsupported semantic contract version")
    require(len(contract.get("differential_cases", [])) >= 12,
            "semantic differential matrix is incomplete")
    require(len(contract.get("invariants", [])) >= 6,
            "semantic invariant matrix is incomplete")
    return contract


def source_corpus(root: Path) -> list[Path]:
    result: list[Path] = []
    for path in root.rglob("*.ns"):
        relative = path.relative_to(root).as_posix()
        if relative.startswith("selfhost/out/") or relative.startswith("selfhost/evidence/"):
            continue
        if any(part in {".git", "_nscbuilder"} for part in path.parts):
            continue
        result.append(path)
    return sorted(result)


def verify(
    root: Path, nsc0: Path, next_o0: Path, next_o2: Path,
    contract_path: Path,
) -> dict:
    contract = load_contract(contract_path)
    actions = contract["actions"]
    differential_comparisons = 0
    for relative in contract["differential_cases"]:
        source = root / relative
        for action in actions:
            legacy = run(nsc0, action, source)
            require(legacy.returncode == 0,
                    f"nsc0 did not produce a semantic oracle: {action} {relative}")
            for executable, optimization in ((next_o0, "O0"), (next_o2, "O2")):
                actual = run(executable, action, source)
                require(actual.returncode == 0,
                        f"{optimization} rejected semantic oracle case: {action} {relative}")
                require(actual.stderr == b"",
                        f"{optimization} wrote semantic stderr: {action} {relative}")
                require(canonical_lines(actual.stdout) == canonical_lines(legacy.stdout),
                        f"{optimization} semantic dump differs from nsc0: {action} {relative}")
                differential_comparisons += 1

    sources = source_corpus(root)
    require(len(sources) >= int(contract["stability_minimum_sources"]),
            f"semantic stability corpus unexpectedly small: {len(sources)}")
    stability_comparisons = 0
    for source in sources:
        relative = source.relative_to(root).as_posix()
        for action in actions:
            o0 = run(next_o0, action, source)
            o2 = run(next_o2, action, source)
            require(o0.returncode in (0, 1) and o2.returncode in (0, 1),
                    f"semantic process crashed: {action} {relative}")
            require(o0.returncode == o2.returncode,
                    f"O0/O2 semantic exit differs: {action} {relative}")
            require(canonical_lines(o0.stdout) == canonical_lines(o2.stdout),
                    f"O0/O2 semantic stdout differs: {action} {relative}")
            require(canonical_lines(o0.stderr) == canonical_lines(o2.stderr),
                    f"O0/O2 semantic stderr differs: {action} {relative}")
            stability_comparisons += 1

    invariant_checks = 0
    for invariant in contract["invariants"]:
        source = root / invariant["source"]
        for executable, optimization in ((next_o0, "O0"), (next_o2, "O2")):
            actual = run(executable, invariant["action"], source)
            require(actual.returncode == 0,
                    f"{optimization} rejected invariant source: {invariant['source']}")
            text = canonical_lines(actual.stdout).decode("utf-8", errors="strict")
            for expected in invariant["contains"]:
                require(expected in text,
                        f"{optimization} missing semantic invariant in {invariant['source']}: {expected}")
                invariant_checks += 1

    return {
        "schema": REPORT_SCHEMA,
        "version": VERSION,
        "status": "passed",
        "optimizations": ["O0", "O2"],
        "source_files": len(sources),
        "differential_case_files": contract["differential_cases"],
        "differential_comparisons": differential_comparisons,
        "stability_comparisons": stability_comparisons,
        "invariant_checks": invariant_checks,
        "binary_sha256": {"O0": digest(next_o0), "O2": digest(next_o2)},
    }


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Verify the self-hosted SH06 semantic model.")
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--nsc0", type=Path, required=True)
    parser.add_argument("--nsc-next-o0", type=Path, required=True)
    parser.add_argument("--nsc-next-o2", type=Path, required=True)
    parser.add_argument("--contract", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    arguments = parser.parse_args(argv)
    try:
        report = verify(
            arguments.source_root.resolve(), arguments.nsc0.resolve(),
            arguments.nsc_next_o0.resolve(), arguments.nsc_next_o2.resolve(),
            arguments.contract.resolve(),
        )
        arguments.report.resolve().write_text(
            json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
    except (OSError, subprocess.SubprocessError, json.JSONDecodeError, VerificationError) as error:
        print(f"verify_semantics: error: {error}", file=sys.stderr)
        return 1
    print(
        f"SH06 semantics verified: {report['source_files']} sources, "
        f"{report['differential_comparisons']} differential, "
        f"{report['stability_comparisons']} stability and "
        f"{report['invariant_checks']} invariant comparisons"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
