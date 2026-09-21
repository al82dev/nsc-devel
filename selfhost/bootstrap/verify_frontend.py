from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path
from typing import Sequence


SCHEMA = "nativesharp-selfhost-frontend-contract"
REPORT_SCHEMA = "nativesharp-selfhost-sh05-report"
VERSION = 1


class VerificationError(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise VerificationError(message)


def run(executable: Path, *arguments: str, timeout: int = 120) -> subprocess.CompletedProcess[bytes]:
    return subprocess.run(
        [str(executable), *arguments],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
        timeout=timeout,
    )


def load_contract(path: Path) -> dict:
    contract = json.loads(path.read_text(encoding="utf-8"))
    require(contract.get("schema") == SCHEMA, "unexpected frontend contract schema")
    require(contract.get("version") == VERSION, "unsupported frontend contract version")
    require(len(contract.get("ast_cases", [])) >= 16, "frontend AST matrix is incomplete")
    require(len(contract.get("recovery", {}).get("cases", [])) >= 5,
            "frontend recovery matrix is incomplete")
    return contract


def source_corpus(root: Path, exclusions: set[str]) -> list[Path]:
    result: list[Path] = []
    for path in root.rglob("*.ns"):
        relative = path.relative_to(root).as_posix()
        if relative in exclusions:
            continue
        if relative.startswith("selfhost/out/") or relative.startswith("selfhost/evidence/"):
            continue
        if any(part in {".git", "_nscbuilder"} for part in path.parts):
            continue
        result.append(path)
    return sorted(result)


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def canonical_lines(data: bytes) -> bytes:
    return data.replace(b"\r\n", b"\n").replace(b"\r", b"\n")


def verify(
    root: Path,
    nsc0: Path,
    next_o0: Path,
    next_o2: Path,
    contract_path: Path,
    output_dir: Path,
) -> dict:
    contract = load_contract(contract_path)
    output_dir.mkdir(parents=True, exist_ok=True)
    exclusions = set(contract["token_exclusions"])
    sources = source_corpus(root, exclusions)
    require(len(sources) >= 470, f"frontend source corpus unexpectedly small: {len(sources)}")

    legacy_tokens: dict[str, bytes] = {}
    for source in sources:
        relative = source.relative_to(root).as_posix()
        legacy_tokens[relative] = canonical_lines(run(nsc0, "--dump-tokens", str(source)).stdout)

    token_cases = 0
    ast_cases = 0
    recovery_cases = 0
    for executable, optimization in ((next_o0, "O0"), (next_o2, "O2")):
        for source in sources:
            relative = source.relative_to(root).as_posix()
            actual = run(executable, "--dump-tokens", str(source))
            require(canonical_lines(actual.stdout) == legacy_tokens[relative],
                    f"{optimization} token dump differs from nsc0: {relative}")
            token_cases += 1

        for relative in contract["ast_cases"]:
            source = root / relative
            legacy = run(nsc0, "--dump-ast", str(source))
            actual = run(executable, "--dump-ast", str(source))
            require(legacy.stdout.startswith(b"CompilationUnit"),
                    f"nsc0 did not produce an AST oracle for {relative}")
            require(actual.returncode == 0, f"{optimization} parser rejected {relative}")
            require(actual.stderr == b"", f"{optimization} parser wrote stderr for {relative}")
            require(canonical_lines(actual.stdout) == canonical_lines(legacy.stdout),
                    f"{optimization} AST dump differs from nsc0: {relative}")
            ast_cases += 1

        recovery = contract["recovery"]
        timeout = int(recovery["timeout_seconds"])
        maximum_lines = int(recovery["maximum_dump_lines"])
        for ordinal, source_text in enumerate(recovery["cases"]):
            fixture = output_dir / f"recovery-{ordinal}.ns"
            fixture.write_text(source_text, encoding="utf-8", newline="\n")
            legacy_tokens_result = run(nsc0, "--dump-tokens", str(fixture), timeout=timeout)
            actual_tokens = run(executable, "--dump-tokens", str(fixture), timeout=timeout)
            require(canonical_lines(actual_tokens.stdout) == canonical_lines(legacy_tokens_result.stdout),
                    f"{optimization} recovery token dump differs in case {ordinal}")
            actual_tree = run(executable, "--dump-ast", str(fixture), timeout=timeout)
            require(actual_tree.returncode != 0,
                    f"{optimization} malformed recovery case {ordinal} was accepted")
            require(actual_tree.stdout.count(b"\n") <= maximum_lines,
                    f"{optimization} recovery case {ordinal} exceeded cascade limit")
            recovery_cases += 1

    return {
        "schema": REPORT_SCHEMA,
        "version": VERSION,
        "status": "passed",
        "optimizations": ["O0", "O2"],
        "source_files": len(sources),
        "token_comparisons": token_cases,
        "ast_comparisons": ast_cases,
        "recovery_comparisons": recovery_cases,
        "ast_case_files": contract["ast_cases"],
        "excluded_token_files": contract["token_exclusions"],
        "binary_sha256": {"O0": digest(next_o0), "O2": digest(next_o2)},
    }


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Verify the self-hosted SH05 frontend.")
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--nsc0", type=Path, required=True)
    parser.add_argument("--nsc-next-o0", type=Path, required=True)
    parser.add_argument("--nsc-next-o2", type=Path, required=True)
    parser.add_argument("--contract", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    arguments = parser.parse_args(argv)
    try:
        report = verify(
            arguments.source_root.resolve(), arguments.nsc0.resolve(),
            arguments.nsc_next_o0.resolve(), arguments.nsc_next_o2.resolve(),
            arguments.contract.resolve(), arguments.output_dir.resolve(),
        )
        arguments.report.resolve().write_text(
            json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
    except (OSError, subprocess.SubprocessError, json.JSONDecodeError, VerificationError) as error:
        print(f"verify_frontend: error: {error}", file=sys.stderr)
        return 1
    print(
        f"SH05 frontend verified: {report['source_files']} sources, "
        f"{report['token_comparisons']} token, {report['ast_comparisons']} AST and "
        f"{report['recovery_comparisons']} recovery comparisons"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
