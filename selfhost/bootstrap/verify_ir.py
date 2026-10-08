from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path
from typing import Callable, Sequence


SCHEMA = "nativesharp-selfhost-ir-contract"
REPORT_SCHEMA = "nativesharp-selfhost-sh07-report"
VERSION = 1


class VerificationError(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise VerificationError(message)


def canonical_lines(data: bytes) -> bytes:
    return data.replace(b"\r\n", b"\n").replace(b"\r", b"\n")


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source_corpus(root: Path, corpus_roots: Sequence[str]) -> list[Path]:
    result: list[Path] = []
    for relative_root in corpus_roots:
        corpus_root = root / relative_root
        require(corpus_root.is_dir(), f"IR corpus root is missing: {relative_root}")
        for path in corpus_root.rglob("*.ns"):
            relative = path.relative_to(root).as_posix()
            if not relative.startswith("tests/cases/"):
                result.append(path)
    return sorted(result)


def run(executable: Path, action: str, source: Path) -> subprocess.CompletedProcess[bytes]:
    return subprocess.run(
        [str(executable), action, str(source)], stdout=subprocess.PIPE,
        stderr=subprocess.PIPE, check=False, timeout=120,
    )


def load_contract(path: Path) -> dict:
    contract = json.loads(path.read_text(encoding="utf-8"))
    require(contract.get("schema") == SCHEMA, "unexpected IR contract schema")
    require(contract.get("version") == VERSION, "unsupported IR contract version")
    require(contract.get("optimizations") == ["O0", "O1", "O2", "O3"],
            "SH07 must exercise O0/O1/O2/O3")
    require(len(contract.get("stages", [])) == 6, "SH07 stage matrix is incomplete")
    require(len(contract.get("cross_stage_sources", [])) >= 6,
            "SH07 cross-stage corpus is incomplete")
    require(int(contract.get("corpus_minimum_sources", 0)) >= 480,
            "SH07 corpus floor is incomplete")
    require(int(contract.get("oracle_ssa_minimum_sources", 0)) >= 20,
            "SH07 standalone SSA oracle floor is incomplete")
    require(len(contract.get("corpus_roots", [])) >= 6,
            "IR corpus roots are incomplete")
    for level in contract["optimizations"]:
        require(level in contract.get("required_pass_order", {}),
                f"missing pass order for {level}")
    return contract


def function_names(text: str, action: str) -> set[str]:
    if action == "--dump-hir":
        # Legacy records symbol=Function:name, SH07 records text="name".
        legacy = set(re.findall(r"FunctionDeclaration[^\n]*symbol=(?:Function|Method):([^\s]+)", text))
        modern = set(re.findall(r"FunctionDeclaration[^\n]*text=\"([^\"]+)\"", text))
        return legacy | modern
    return set(re.findall(r"^function @([^\s:]+)", text, flags=re.MULTILINE))


def reduce_lines(source: str, still_fails: Callable[[str], bool]) -> str:
    """Deterministic line-based ddmin used for the first structural divergence."""
    lines = source.splitlines(keepends=True)
    if len(lines) < 2 or not still_fails(source):
        return source
    granularity = 2
    while len(lines) >= 2:
        chunk = max(1, len(lines) // granularity)
        reduced = False
        start = 0
        while start < len(lines):
            candidate = lines[:start] + lines[start + chunk:]
            if candidate and still_fails("".join(candidate)):
                lines = candidate
                granularity = max(2, granularity - 1)
                reduced = True
                break
            start += chunk
        if not reduced:
            if granularity >= len(lines):
                break
            granularity = min(len(lines), granularity * 2)
    return "".join(lines)


def materialize_first_divergence(
    source: Path, nsc0: Path, candidate: Path, action: str, output_dir: Path,
) -> Path:
    """Reduce and persist the first function-set divergence without touching the source tree."""
    output_dir.mkdir(parents=True, exist_ok=True)
    work = output_dir / "sh07-reducer-work.ns"
    original = source.read_text(encoding="utf-8")

    def still_fails(text: str) -> bool:
        work.write_text(text, encoding="utf-8", newline="\n")
        legacy = run(nsc0, action, work)
        modern = run(candidate, action, work)
        if legacy.returncode != 0 or modern.returncode != 0:
            return False
        legacy_text = canonical_lines(legacy.stdout).decode("utf-8", errors="replace")
        modern_text = canonical_lines(modern.stdout).decode("utf-8", errors="replace")
        return function_names(legacy_text, action) != function_names(modern_text, action)

    reduced = reduce_lines(original, still_fails)
    reproducer = output_dir / "sh07-first-divergence.ns"
    reproducer.write_text(reduced, encoding="utf-8", newline="\n")
    metadata = {
        "schema": "nativesharp-selfhost-sh07-divergence",
        "version": 1,
        "action": action,
        "source": source.as_posix(),
        "original_bytes": len(original.encode("utf-8")),
        "reduced_bytes": len(reduced.encode("utf-8")),
        "reproducer": reproducer.name,
    }
    (output_dir / "sh07-first-divergence.json").write_text(
        json.dumps(metadata, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    work.unlink(missing_ok=True)
    return reproducer


def verify(
    root: Path, nsc0: Path, candidates: dict[str, Path], contract_path: Path,
    divergence_dir: Path | None = None,
) -> dict:
    contract = load_contract(contract_path)
    selected = list(candidates)
    require(bool(selected), "IR verifier requires at least one selected profile")
    require(all(level in contract["optimizations"] for level in selected),
            "IR verifier received a profile outside the contract")
    structural_comparisons = 0
    stability_comparisons = 0
    verifier_checks = 0
    invariant_checks = 0
    corpus_stability_comparisons = 0
    oracle_ssa_sources = 0

    for stage in contract["stages"]:
        source = root / stage["source"]
        legacy = run(nsc0, stage["dump"], source)
        require(legacy.returncode == 0,
                f"nsc0 did not produce {stage['dump']} oracle for {stage['source']}")
        legacy_text = canonical_lines(legacy.stdout).decode("utf-8", errors="strict")
        require(legacy_text.startswith(stage["legacy_header"]),
                f"nsc0 {stage['dump']} header changed")
        reference: bytes | None = None
        for level in selected:
            actual = run(candidates[level], stage["dump"], source)
            require(actual.returncode == 0, f"{level} rejected {stage['dump']} {stage['source']}")
            require(actual.stderr == b"", f"{level} wrote stderr for {stage['dump']}")
            normalized = canonical_lines(actual.stdout)
            text = normalized.decode("utf-8", errors="strict")
            require(text.startswith(stage["header"]), f"{level} has invalid {stage['dump']} header")
            for expected in stage["contains"]:
                require(expected in text, f"{level} {stage['dump']} misses invariant {expected!r}")
                invariant_checks += 1
            if stage["dump"] != "--dump-pass-pipeline":
                same_functions = function_names(text, stage["dump"]) == function_names(legacy_text, stage["dump"])
                if not same_functions and divergence_dir is not None:
                    reproducer = materialize_first_divergence(
                        source, nsc0, candidates[level], stage["dump"], divergence_dir
                    )
                    raise VerificationError(
                        f"{level} function set differs from nsc0 in {stage['dump']}; "
                        f"reduced reproducer: {reproducer}"
                    )
                require(same_functions, f"{level} function set differs from nsc0 in {stage['dump']}")
                structural_comparisons += 1
            if reference is None:
                reference = normalized
            else:
                require(normalized == reference, f"{stage['dump']} is unstable across optimization levels")
                stability_comparisons += 1
            verified = run(candidates[level], stage["verify"], source)
            require(verified.returncode == 0 and verified.stdout == b"" and verified.stderr == b"",
                    f"{level} verifier rejected {stage['source']} at {stage['verify']}")
            verifier_checks += 1

    actions = [stage["dump"] for stage in contract["stages"][:-1]]
    for relative in contract["cross_stage_sources"]:
        source = root / relative
        for action in actions:
            baseline: tuple[int, bytes, bytes] | None = None
            for level in selected:
                result = run(candidates[level], action, source)
                observed = (result.returncode, canonical_lines(result.stdout), canonical_lines(result.stderr))
                require(result.returncode in (0, 1), f"{level} crashed in {action} {relative}")
                if baseline is None:
                    baseline = observed
                else:
                    require(observed == baseline, f"{action} {relative} differs across O0/O1/O2/O3")
                    stability_comparisons += 1

    sources = source_corpus(root, contract["corpus_roots"])
    require(len(sources) >= int(contract["corpus_minimum_sources"]),
            f"SH07 source corpus unexpectedly small: {len(sources)}")
    for source in sources:
        legacy = run(nsc0, "--verify-ssa", source)
        baseline: tuple[int, bytes, bytes] | None = None
        for level in selected:
            actual = run(candidates[level], "--verify-ssa", source)
            require(actual.returncode in (0, 1),
                    f"{level} crashed while reaching SSA for {source.relative_to(root).as_posix()}")
            observed = (actual.returncode, canonical_lines(actual.stdout), canonical_lines(actual.stderr))
            if baseline is None:
                baseline = observed
            else:
                require(observed == baseline,
                        f"SSA reachability differs across levels for {source.relative_to(root).as_posix()}")
                corpus_stability_comparisons += 1
            if legacy.returncode == 0:
                require(actual.returncode == 0,
                        f"{level} failed standalone SSA oracle source {source.relative_to(root).as_posix()}")
        if legacy.returncode == 0:
            oracle_ssa_sources += 1
    require(oracle_ssa_sources >= int(contract["oracle_ssa_minimum_sources"]),
            f"standalone SSA oracle corpus unexpectedly small: {oracle_ssa_sources}")

    pipeline = run(candidates[selected[0]], "--dump-pass-pipeline",
                   root / contract["stages"][-1]["source"])
    pipeline_text = canonical_lines(pipeline.stdout).decode("utf-8", errors="strict")
    for level, passes in contract["required_pass_order"].items():
        line = next((value for value in pipeline_text.splitlines() if value.startswith(level + ":")), "")
        position = -1
        for pass_name in passes:
            found = line.find(pass_name, position + 1)
            require(found > position, f"{level} pass order omits or reorders {pass_name}")
            position = found
            invariant_checks += 1

    return {
        "schema": REPORT_SCHEMA,
        "version": VERSION,
        "status": "passed",
        "optimizations": selected,
        "stage_count": len(contract["stages"]),
        "cross_stage_sources": len(contract["cross_stage_sources"]),
        "source_files": len(sources),
        "oracle_ssa_sources": oracle_ssa_sources,
        "corpus_stability_comparisons": corpus_stability_comparisons,
        "structural_comparisons": structural_comparisons,
        "stability_comparisons": stability_comparisons,
        "verifier_checks": verifier_checks,
        "invariant_checks": invariant_checks,
        "binary_sha256": {level: digest(path) for level, path in candidates.items()},
    }


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Verify self-hosted SH07 intermediate representations.")
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
        report_path = arguments.report.resolve()
        report = verify(arguments.source_root.resolve(), arguments.nsc0.resolve(), candidates,
                        arguments.contract.resolve(), report_path.parent)
        report_path.write_text(
            json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    except (OSError, subprocess.SubprocessError, json.JSONDecodeError, VerificationError) as error:
        print(f"verify_ir: error: {error}", file=sys.stderr)
        return 1
    print(
        "SH07 IR verified: "
        f"{report['structural_comparisons']} structural, "
        f"{report['stability_comparisons']} stability, "
        f"{report['verifier_checks']} verifier and {report['invariant_checks']} invariant checks"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
