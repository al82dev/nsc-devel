from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path
from typing import Sequence


CONTRACT_SCHEMA = "nativesharp-diagnostics-map"
REPORT_SCHEMA = "nativesharp-selfhost-sh04-report"
VERSION = 1


class VerificationError(RuntimeError):
    pass


def run(executable: Path, *arguments: str, cwd: Path | None = None) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [str(executable), *arguments],
        cwd=cwd,
        text=True,
        encoding="utf-8",
        errors="strict",
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )


def require(condition: bool, message: str) -> None:
    if not condition:
        raise VerificationError(message)


def fnv1a64(text: str) -> int:
    value = 14695981039346656037
    for byte in text.encode("utf-8"):
        value ^= byte
        value = (value * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return value


def load_contract(path: Path) -> dict:
    contract = json.loads(path.read_text(encoding="utf-8"))
    require(contract.get("schema") == CONTRACT_SCHEMA, "unexpected diagnostics map schema")
    require(contract.get("version") == VERSION, "unsupported diagnostics map version")
    diagnostics = contract.get("diagnostics")
    require(isinstance(diagnostics, list), "diagnostics map requires an array")
    require([item.get("code") for item in diagnostics] == ["NSC0001", "NSC0002"],
            "SH04 diagnostics must remain ordered and complete")
    require([item.get("exit_code") for item in diagnostics] == [1, 2],
            "SH04 exit codes changed")
    return contract


def verify(
    nsc0: Path,
    next_o0: Path,
    next_o2: Path,
    contract_path: Path,
    output_dir: Path,
) -> dict:
    contract = load_contract(contract_path)
    output_dir.mkdir(parents=True, exist_ok=True)
    left_cwd = output_dir / "cwd one"
    right_cwd = output_dir / "cwd two"
    fixture_dir = output_dir / "path with spaces"
    left_cwd.mkdir(exist_ok=True)
    right_cwd.mkdir(exist_ok=True)
    fixture_dir.mkdir(exist_ok=True)
    fixture = fixture_dir / "mixed endings.ns"
    fixture.write_bytes(b"first\r\nsecond\rthird\n")
    missing = (fixture_dir / "missing source.ns").resolve()

    for executable, optimization in ((next_o0, "O0"), (next_o2, "O2")):
        version = run(executable, "--version")
        require(version.returncode == 0, f"{optimization} --version failed")
        require(version.stdout ==
                "nsc-next 2.0.0-bootstrap.sh14 (NativeSharp 2.0.0, promotion candidate)\n",
                f"{optimization} identity changed")
        help_result = run(executable, "--help")
        require(help_result.returncode == 0 and "Usage:" in help_result.stdout,
                f"{optimization} --help failed")
        self_test = run(executable, "--self-test-basic")
        require(self_test.returncode == 42 and "self-test passed" in self_test.stdout,
                f"{optimization} Basic/Source/Diagnostics self-test failed")

    legacy_missing = run(nsc0, "--no-banner", str(missing))
    legacy_unknown = run(nsc0, "--no-banner", "--sh04-unknown-option")
    require(legacy_missing.returncode == 1, "nsc0 missing-file contract changed")
    require(legacy_unknown.returncode == 2, "nsc0 unknown-option contract changed")

    dumps: list[dict] = []
    binary_hashes: dict[str, str] = {}
    for executable, optimization in ((next_o0, "O0"), (next_o2, "O2")):
        missing_result = run(executable, str(missing))
        require(missing_result.returncode == legacy_missing.returncode,
                f"{optimization} missing-file exit differs from nsc0")
        require(missing_result.stdout == legacy_missing.stdout and
                missing_result.stderr == legacy_missing.stderr,
                f"{optimization} missing-file text differs from nsc0")

        unknown_result = run(executable, "--sh04-unknown-option")
        require(unknown_result.returncode == legacy_unknown.returncode,
                f"{optimization} unknown-option exit differs from nsc0")
        require(unknown_result.stdout == legacy_unknown.stdout and
                unknown_result.stderr == legacy_unknown.stderr,
                f"{optimization} unknown-option text differs from nsc0")

        first = run(executable, "--dump-source-info", str(fixture.resolve()), cwd=left_cwd)
        second = run(executable, "--dump-source-info", str(fixture.resolve()), cwd=right_cwd)
        require(first.returncode == 0 and first.stderr == "", f"{optimization} source read failed")
        require(first.stdout == second.stdout, f"{optimization} source info depends on cwd")
        source_info = json.loads(first.stdout)
        require(source_info.get("schema") == "nativesharp-source-info" and
                source_info.get("version") == 1, f"{optimization} source schema changed")
        require(source_info.get("bytes") == 19 and source_info.get("lines") == 4,
                f"{optimization} line-ending normalization changed")
        require(source_info.get("fnv1a64") == str(fnv1a64("first\nsecond\nthird\n")),
                f"{optimization} stable content hash changed")
        require("\\" not in source_info.get("path", ""),
                f"{optimization} diagnostic path was not normalized")
        dumps.append(source_info)

        structured = run(
            executable, "--diagnostic-format=json", str(missing), cwd=left_cwd
        )
        require(structured.returncode == 1 and structured.stdout == "",
                f"{optimization} structured missing-file result changed")
        diagnostic = json.loads(structured.stderr)
        require(diagnostic.get("schema") == "nativesharp-diagnostic" and
                diagnostic.get("version") == 1 and diagnostic.get("code") == "NSC0001",
                f"{optimization} structured diagnostic contract changed")
        require("\\" not in diagnostic.get("argument", ""),
                f"{optimization} structured diagnostic path was not normalized")
        binary_hashes[optimization] = hashlib.sha256(executable.read_bytes()).hexdigest()

    require(dumps[0] == dumps[1], "O0 and O2 source behavior differs")
    return {
        "schema": REPORT_SCHEMA,
        "version": VERSION,
        "status": "passed",
        "identity": "nsc-next 2.0.0-bootstrap.sh14",
        "optimizations": ["O0", "O2"],
        "nsc0_differential_cases": 2,
        "source_cases": 2,
        "cwd_independent": True,
        "line_endings": ["CRLF", "CR", "LF"],
        "diagnostics": [item["code"] for item in contract["diagnostics"]],
        "binary_sha256": binary_hashes,
        "source_info": dumps[0],
    }


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Verify the nsc-next SH04 skeleton.")
    parser.add_argument("--nsc0", type=Path, required=True)
    parser.add_argument("--nsc-next-o0", type=Path, required=True)
    parser.add_argument("--nsc-next-o2", type=Path, required=True)
    parser.add_argument("--contract", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    arguments = parser.parse_args(argv)
    try:
        report = verify(
            arguments.nsc0.resolve(),
            arguments.nsc_next_o0.resolve(),
            arguments.nsc_next_o2.resolve(),
            arguments.contract.resolve(),
            arguments.output_dir.resolve(),
        )
        arguments.report.resolve().write_text(
            json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
    except (OSError, UnicodeError, json.JSONDecodeError, VerificationError) as error:
        print(f"verify_nsc_next_skeleton: error: {error}", file=sys.stderr)
        return 1
    print("SH04 nsc-next skeleton verified: O0/O2, diagnostics, source maps and paths")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
