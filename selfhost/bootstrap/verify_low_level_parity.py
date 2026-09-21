#!/usr/bin/env python3
"""SH45: prove the self-hosted compiler's NativeSharp 2.0 low-level surface."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path


def run(command: list[str], *, cwd: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        command,
        cwd=cwd,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
    )


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", required=True, type=Path)
    parser.add_argument("--contract", required=True, type=Path)
    parser.add_argument("--compiler", required=True, type=Path)
    parser.add_argument("--nslink", required=True, type=Path)
    parser.add_argument("--lld-link", required=True, type=Path)
    parser.add_argument("--llvm-nm", required=True, type=Path)
    parser.add_argument("--llvm-objdump", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()

    root = args.source_root.resolve()
    contract = json.loads(args.contract.read_text(encoding="utf-8"))
    if contract.get("schema") != "nativesharp-selfhost-low-level-parity-contract" or contract.get("version") != 1:
        print("NSH4501: unsupported SH45 contract", file=sys.stderr)
        return 1
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    failures: list[str] = []
    cases: list[dict[str, object]] = []

    import_libraries: list[Path] = []
    for dll, symbols in contract["imports"].items():
        library = output / (dll.replace(".", "_") + ".lib")
        description = dll + ":" + ",".join(symbols)
        result = run([str(args.nslink), "--link", "--make-implib", description, str(library)], cwd=root)
        if result.returncode != 0:
            failures.append(f"import library {dll}: {result.stdout}{result.stderr}")
        else:
            import_libraries.append(library)

    full_project = root / "selfhost/tests/low-level-2.0/LowLevel.nsproj"
    metadata = run([str(args.compiler), "--dump-project-layout", str(full_project)], cwd=root)
    if metadata.returncode != 0:
        failures.append("semantic metadata: " + metadata.stdout + metadata.stderr)
    else:
        for fragment in contract["metadata_fragments"]:
            if fragment not in metadata.stdout:
                failures.append(f"semantic metadata lost {fragment!r}")

    for profile in contract["profiles"]:
        profile_flag = "-" + profile
        for project_text in contract["positive_projects"]:
            project = root / project_text
            stem = project.stem.lower()
            object_path = output / f"{stem}-{profile.lower()}.obj"
            emitted = run(
                [str(args.compiler), "--emit-project-object", str(project), profile_flag, "-o", str(object_path)],
                cwd=root,
            )
            if emitted.returncode != 0:
                failures.append(f"{project_text} {profile}: object emission failed: {emitted.stdout}{emitted.stderr}")
                continue

            symbols = run([str(args.llvm_nm), str(object_path)], cwd=root)
            for forbidden in contract["forbidden_undefined_symbol_fragments"]:
                if forbidden in symbols.stdout:
                    failures.append(f"{project_text} {profile}: compiler emitted fallback symbol {forbidden}")
            if "atomic" in project_text.casefold() or project == full_project:
                assembly = run([str(args.llvm_objdump), "-d", str(object_path)], cwd=root)
                lowered = assembly.stdout.casefold()
                for fragment in contract["atomic_disassembly_fragments"]:
                    if fragment.casefold() not in lowered:
                        failures.append(f"{project_text} {profile}: atomic machine code lost {fragment!r}")

            for linker in contract["linkers"]:
                executable = output / f"{stem}-{profile.lower()}-{linker}.exe"
                if executable.exists():
                    executable.unlink()
                if linker == "nslink":
                    command = [str(args.nslink), "--link", str(object_path), "--entry", contract["entry"]]
                    for dll, imported in contract["imports"].items():
                        command += ["--import", dll + ":" + ",".join(imported)]
                    command += ["-o", str(executable)]
                else:
                    command = [
                        str(args.lld_link), "/nologo", "/subsystem:console", "/machine:x64",
                        "/timestamp:0", "/entry:" + contract["entry"], "/out:" + str(executable),
                        str(object_path), *map(str, import_libraries),
                    ]
                linked = run(command, cwd=root)
                if linked.returncode != 0:
                    failures.append(f"{project_text} {profile} {linker}: link failed: {linked.stdout}{linked.stderr}")
                    continue
                executed = run([str(executable)], cwd=root)
                if executed.returncode != 0:
                    failures.append(f"{project_text} {profile} {linker}: exit={executed.returncode}")
                    continue
                cases.append(
                    {
                        "project": project_text,
                        "profile": profile,
                        "linker": linker,
                        "object_sha256": sha256(object_path),
                        "image_sha256": sha256(executable),
                        "status": "passed",
                    }
                )

        for negative in contract["negative_projects"]:
            project = root / negative["project"]
            rejected_object = output / f"negative-{project.stem.lower()}-{profile.lower()}.obj"
            rejected = run(
                [str(args.compiler), "--emit-project-object", str(project), profile_flag, "-o", str(rejected_object)],
                cwd=root,
            )
            combined = rejected.stdout + rejected.stderr
            if rejected.returncode == 0 or negative["diagnostic"] not in combined:
                failures.append(
                    f"{negative['project']} {profile}: expected rejection containing {negative['diagnostic']!r}; "
                    f"exit={rejected.returncode}: {combined}"
                )

    report = {
        "schema": "nativesharp-selfhost-low-level-parity-report",
        "version": 1,
        "status": "failed" if failures else "passed",
        "compiler": str(args.compiler.resolve()),
        "compiler_sha256": sha256(args.compiler),
        "contract_sha256": sha256(args.contract),
        "profiles": contract["profiles"],
        "linkers": contract["linkers"],
        "passed_executions": len(cases),
        "expected_executions": len(contract["profiles"]) * len(contract["positive_projects"]) * len(contract["linkers"]),
        "cases": cases,
        "failures": failures,
    }
    report_path = args.report or (output / "report.json")
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if failures:
        for failure in failures:
            print("NSH4502: " + failure, file=sys.stderr)
        return 1
    print(
        f"SH45 low-level parity verified: {len(cases)} O0-O3 LLD/NSLink executions, "
        f"{len(contract['negative_projects']) * len(contract['profiles'])} contractual rejections"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
