#!/usr/bin/env python3
"""Verify SH43 nsc2->nsc3->nsc4 fixed point in three clean repetitions."""
from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path


SCHEMA = "nativesharp-selfhost-repeated-fixed-point-contract"
REPORT_SCHEMA = "nativesharp-selfhost-sh43-report"


class VerificationError(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise VerificationError(message)


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command: list[str], environment: dict[str, str] | None = None,
        timeout: int = 900) -> tuple[subprocess.CompletedProcess[str], float]:
    started = time.perf_counter()
    result = subprocess.run(command, env=environment, text=True, encoding="utf-8",
                            errors="replace", stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            check=False, timeout=timeout)
    return result, time.perf_counter() - started


def checked(command: list[str], label: str, environment: dict[str, str] | None = None) -> float:
    result, elapsed = run(command, environment)
    require(result.returncode == 0,
            f"{label} failed ({result.returncode}): {result.stdout}{result.stderr}")
    return elapsed


def link(tool: Path, obj: Path, libraries: list[Path], output: Path) -> float:
    command = [str(tool), "--link", str(obj)]
    for library in libraries:
        command.extend(["--lib", str(library)])
    command.extend(["--entry", "mainCRTStartup", "-o", str(output)])
    elapsed = checked(command, f"NSLink link {output.name}")
    require(output.is_file() and output.stat().st_size > 0, f"missing image {output}")
    return elapsed


def verify_image(image: Path, identity: str, project: Path) -> dict[str, object]:
    version, version_time = run([str(image), "--version"])
    require(version.returncode == 0 and version.stdout.strip() == identity,
            f"identity mismatch in {image}: {version.stdout}{version.stderr}")
    self_test, self_test_time = run([str(image), "--self-test-basic"])
    require(self_test.returncode == 42, f"self-test failed in {image}")
    project_test, project_time = run([str(image), "--check-project", str(project)])
    require(project_test.returncode == 0,
            f"project verification failed in {image}: {project_test.stdout}{project_test.stderr}")
    return {"version_seconds": round(version_time, 6),
            "self_test_seconds": round(self_test_time, 6),
            "project_seconds": round(project_time, 6)}


def execute_case(repetition: int, profile: str, nsc2: Path, project: Path,
                 identity: str, nslink: Path, libraries: list[Path],
                 output_root: Path, environment: dict[str, str]) -> dict[str, object]:
    case = output_root / f"repetition-{repetition}" / profile.lower()
    require(not case.exists(), f"clean repetition output already exists: {case}")
    case.mkdir(parents=True)
    nsc3_object = case / "nsc3.obj"
    nsc4_object = case / "nsc4.obj"
    emit3 = checked([str(nsc2), "--emit-bootstrap-object", str(project), f"-{profile}",
                     "-o", str(nsc3_object)], f"repetition {repetition}/{profile} nsc3", environment)
    require(nsc3_object.is_file(), f"nsc3 object missing in repetition {repetition}/{profile}")

    images3 = {"nslink": case / "nsc3-nslink.exe"}
    link_times3 = {"nslink": link(nslink, nsc3_object, libraries, images3["nslink"])}
    validation3 = verify_image(images3["nslink"], identity, project)

    emit4 = checked([str(images3["nslink"]), "--emit-bootstrap-object", str(project),
                     f"-{profile}", "-o", str(nsc4_object)],
                    f"repetition {repetition}/{profile} nsc4", environment)
    require(nsc4_object.is_file(), f"nsc4 object missing in repetition {repetition}/{profile}")
    require(nsc3_object.read_bytes() == nsc4_object.read_bytes(),
            f"raw object bytes differ in repetition {repetition}/{profile}")

    images4 = {"nslink": case / "nsc4-nslink.exe"}
    link_times4 = {"nslink": link(nslink, nsc4_object, libraries, images4["nslink"])}
    require(images3["nslink"].read_bytes() == images4["nslink"].read_bytes(),
            f"raw NSLink image bytes differ in repetition {repetition}/{profile}")
    validation4 = verify_image(images4["nslink"], identity, project)
    return {
        "repetition": repetition, "profile": profile,
        "clean_root": case.as_posix(), "normalization_applied": [],
        "object_byte_identical": True,
        "object_sha256": digest(nsc3_object),
        "image_byte_identical": {route: True for route in images3},
        "image_sha256": {route: digest(image) for route, image in images3.items()},
        "timings_seconds": {"emit_nsc3": round(emit3, 6), "emit_nsc4": round(emit4, 6),
                            "link_nsc3": {key: round(value, 6) for key, value in link_times3.items()},
                            "link_nsc4": {key: round(value, 6) for key, value in link_times4.items()}},
        "nsc3_validation": validation3, "nsc4_validation": validation4,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Verify NativeSharp SH43 repeated fixed point.")
    parser.add_argument("--contract", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    for level in ("o0", "o1", "o2", "o3"):
        parser.add_argument(f"--nsc2-{level}", type=Path, required=True)
    parser.add_argument("--nslink", type=Path, required=True)
    parser.add_argument("--runtime-library", type=Path, required=True)
    parser.add_argument("--llvm-library", type=Path, required=True)
    parser.add_argument("--import-library", action="append", type=Path, default=[])
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--workers", type=int, default=4)
    args = parser.parse_args()
    try:
        contract = json.loads(args.contract.read_text(encoding="utf-8"))
        require(contract.get("schema") == SCHEMA and contract.get("version") == 1,
                "unsupported SH43 contract")
        require(contract.get("repetitions") == 3, "SH43 requires three repetitions")
        require(contract.get("identity_policy", {}).get("normalization") == [],
                "SH43 forbids normalization")
        require(1 <= args.workers <= 16, "workers must be in 1..16")
        root = args.source_root.resolve()
        output = args.output_dir.resolve()
        require(not output.exists(), f"SH43 output root is not clean: {output}")
        output.mkdir(parents=True)
        project = root / contract["compiler_project"]
        candidates = {profile: getattr(args, f"nsc2_{profile.lower()}").resolve()
                      for profile in contract["optimizations"]}
        libraries = [args.runtime_library.resolve(), args.llvm_library.resolve(),
                     *(path.resolve() for path in args.import_library)]
        require(all(path.is_file() for path in [*candidates.values(), args.nslink,
                                                *libraries]), "SH43 input artifact is missing")
        environment = os.environ.copy()
        environment.update({"LANG": "C", "LC_ALL": "C", "TZ": "UTC"})

        jobs = [(repetition, profile) for repetition in range(1, 4)
                for profile in contract["optimizations"]]
        records: list[dict[str, object]] = []
        with ThreadPoolExecutor(max_workers=args.workers) as executor:
            pending = [executor.submit(
                execute_case, repetition, profile, candidates[profile], project,
                contract["expected_identity"], args.nslink.resolve(),
                libraries, output, environment) for repetition, profile in jobs]
            for future in as_completed(pending):
                records.append(future.result())
        records.sort(key=lambda item: (int(item["repetition"]), str(item["profile"])))

        for profile in contract["optimizations"]:
            selected = [item for item in records if item["profile"] == profile]
            require(len({str(item["object_sha256"]) for item in selected}) == 1,
                    f"{profile} object differs across clean repetitions")
            for route in contract["linkers"]:
                require(len({str(item["image_sha256"][route]) for item in selected}) == 1,
                        f"{profile}/{route} image differs across clean repetitions")

        report = {
            "schema": REPORT_SCHEMA, "version": 1, "status": "passed",
            "gate": "SH43", "tanda": 45,
            "repetitions": 3, "profiles": contract["optimizations"],
            "linkers": contract["linkers"], "cases": records, "case_count": len(records),
            "object_comparisons": len(records) * 2,
            "image_comparisons": len(records) * len(contract["linkers"]) * 2,
            "normalization_applied": [], "raw_byte_identity": True,
            "clean_roots": len(records), "skipped": 0, "problems": [],
            "input_sha256": {profile: digest(path) for profile, path in candidates.items()},
        }
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n",
                               encoding="utf-8", newline="\n")
    except (OSError, UnicodeError, ValueError, json.JSONDecodeError,
            subprocess.SubprocessError, VerificationError) as error:
        print(f"verify_repeated_fixed_point: error: {error}", file=sys.stderr)
        return 1
    print("SH43 passed: three clean nsc2->nsc3->nsc4 raw-byte fixed points, O0-O3, NSLink")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
