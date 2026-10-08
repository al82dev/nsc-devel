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
    require(len(contract.get("corpus_roots", [])) >= 6,
            "semantic corpus roots are incomplete")
    return contract


def source_corpus(root: Path, corpus_roots: Sequence[str]) -> list[Path]:
    result: list[Path] = []
    for relative_root in corpus_roots:
        corpus_root = root / relative_root
        require(corpus_root.is_dir(), f"semantic corpus root is missing: {relative_root}")
        result.extend(corpus_root.rglob("*.ns"))
    return sorted(result)


def verify(
    root: Path, nsc0: Path, candidates: dict[str, Path],
    contract_path: Path,
) -> dict:
    contract = load_contract(contract_path)
    require(bool(candidates), "semantic verifier requires at least one selected profile")
    actions = contract["actions"]
    differential_comparisons = 0
    for relative in contract["differential_cases"]:
        source = root / relative
        for action in actions:
            legacy = run(nsc0, action, source)
            require(legacy.returncode == 0,
                    f"nsc0 did not produce a semantic oracle: {action} {relative}")
            for optimization, executable in candidates.items():
                actual = run(executable, action, source)
                require(actual.returncode == 0,
                        f"{optimization} rejected semantic oracle case: {action} {relative}")
                require(actual.stderr == b"",
                        f"{optimization} wrote semantic stderr: {action} {relative}")
                require(canonical_lines(actual.stdout) == canonical_lines(legacy.stdout),
                        f"{optimization} semantic dump differs from nsc0: {action} {relative}")
                differential_comparisons += 1

    sources = source_corpus(root, contract["corpus_roots"])
    require(len(sources) >= int(contract["stability_minimum_sources"]),
            f"semantic stability corpus unexpectedly small: {len(sources)}")
    stability_comparisons = 0
    for source in sources:
        relative = source.relative_to(root).as_posix()
        for action in actions:
            baseline: tuple[int, bytes, bytes] | None = None
            for optimization, executable in candidates.items():
                result = run(executable, action, source)
                require(result.returncode in (0, 1),
                        f"{optimization} semantic process crashed: {action} {relative}")
                observed = (
                    result.returncode,
                    canonical_lines(result.stdout),
                    canonical_lines(result.stderr),
                )
                if baseline is None:
                    baseline = observed
                else:
                    require(observed == baseline,
                            f"semantic output differs across selected profiles: {action} {relative}")
                    stability_comparisons += 1

    invariant_checks = 0
    for invariant in contract["invariants"]:
        source = root / invariant["source"]
        for optimization, executable in candidates.items():
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
        "optimizations": list(candidates),
        "source_files": len(sources),
        "differential_case_files": contract["differential_cases"],
        "differential_comparisons": differential_comparisons,
        "stability_comparisons": stability_comparisons,
        "invariant_checks": invariant_checks,
        "binary_sha256": {profile: digest(path) for profile, path in candidates.items()},
    }


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Verify the self-hosted SH06 semantic model.")
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--nsc0", type=Path, required=True)
    for level in ("o0", "o1", "o2", "o3"):
        parser.add_argument(f"--nsc-next-{level}", type=Path)
    parser.add_argument("--contract", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    arguments = parser.parse_args(argv)
    candidates = {
        level.upper(): value.resolve()
        for level in ("o0", "o1", "o2", "o3")
        if (value := getattr(arguments, f"nsc_next_{level}")) is not None
    }
    try:
        report = verify(
            arguments.source_root.resolve(), arguments.nsc0.resolve(),
            candidates, arguments.contract.resolve(),
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
