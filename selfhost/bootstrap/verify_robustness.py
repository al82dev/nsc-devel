#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import random
import statistics
import subprocess
import sys
import time
from pathlib import Path


SCHEMA = "nativesharp-selfhost-robustness-contract"


class Error(RuntimeError): pass


def require(value: bool, message: str) -> None:
    if not value: raise Error(message)


def run(executable: Path, *args: str) -> tuple[subprocess.CompletedProcess[str], float]:
    started = time.perf_counter()
    result = subprocess.run([str(executable), *args], text=True, encoding="utf-8", errors="replace",
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
    return result, time.perf_counter() - started


def median_startup(executable: Path, iterations: int) -> float:
    return statistics.median(run(executable, "--version")[1] for _ in range(iterations))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--contract", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--nsc0", type=Path, required=True)
    parser.add_argument("--nsc2", type=Path, required=True)
    parser.add_argument("--nsc3", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    try:
        contract = json.loads(args.contract.read_text(encoding="utf-8"))
        require(contract.get("schema") == SCHEMA, "bad robustness contract")
        args.output_dir.mkdir(parents=True, exist_ok=True)
        timings = {name: median_startup(path, contract["timing_iterations"])
                   for name, path in (("nsc0_startup", args.nsc0), ("nsc2_startup", args.nsc2), ("nsc3_startup", args.nsc3))}
        startup_ratio = timings["nsc3_startup"] / max(timings["nsc0_startup"], 0.000001)
        require(startup_ratio <= contract["maximum_startup_ratio"], f"startup regression {startup_ratio:.2f}x")

        rng = random.Random(0x4E534332)
        fuzz_dir = args.output_dir / "fuzz"; fuzz_dir.mkdir(exist_ok=True)
        fuzz_results: list[dict[str, object]] = []
        alphabet = "{}[]();,+-*/<>!= abcdefghijklmnopqrstuvwxyz0123456789\n\t\""
        for index in range(contract["fuzz_cases"]):
            payload = "".join(rng.choice(alphabet) for _ in range(64 + index * 7))
            source = fuzz_dir / f"hostile-{index:02d}.ns"; source.write_text(payload, encoding="utf-8")
            result, elapsed = run(args.nsc3, "--dump-tokens", str(source))
            require(result.returncode in (0, 1), f"hostile input returned {result.returncode}")
            fuzz_results.append({"case": index, "exit": result.returncode, "seconds": round(elapsed, 6),
                                 "sha256": hashlib.sha256(source.read_bytes()).hexdigest()})

        corrupt = args.output_dir / "corrupt.nsmeta"; corrupt.write_bytes(b"NSMETA\0\xffbroken")
        metadata, _ = run(args.nsc3, "--verify-nsmeta", str(corrupt))
        require(metadata.returncode != 0, "corrupt metadata was accepted")
        missing_output = args.output_dir / "missing-parent" / "artifact.obj"
        project = args.source_root / "selfhost/tests/projects/Sh09App/Sh09App.nsproj"
        disk_failure, _ = run(args.nsc3, "--emit-project-object", str(project), "-O0", "-o", str(missing_output))
        require(disk_failure.returncode != 0 and not missing_output.exists(), "output failure was not atomic")

        compile_times: dict[str, dict[str, float]] = {}
        artifacts: dict[str, str] = {}
        for profile in contract["profiles"]:
            per_compiler: dict[str, float] = {}
            for label, compiler in (("nsc2", args.nsc2), ("nsc3", args.nsc3)):
                samples: list[float] = []
                output = args.output_dir / f"{label}-{profile.lower()}.obj"
                for _ in range(3):
                    result, elapsed = run(compiler, "--emit-project-object", str(project), f"-{profile}", "-o", str(output))
                    require(result.returncode == 0, f"{label}/{profile} failed")
                    samples.append(elapsed)
                per_compiler[label] = statistics.median(samples)
                artifacts[output.name] = hashlib.sha256(output.read_bytes()).hexdigest()
            ratio = per_compiler["nsc3"] / max(per_compiler["nsc2"], 0.000001)
            require(ratio <= contract["maximum_compile_ratio"], f"{profile} compile regression {ratio:.2f}x")
            compile_times[profile] = {**per_compiler, "ratio": ratio}

        size_ratio = args.nsc3.stat().st_size / max(args.nsc0.stat().st_size, 1)
        require(size_ratio <= contract["maximum_binary_size_ratio"], f"binary size regression {size_ratio:.2f}x")
        large = args.output_dir / "interrupt.ns"; large.write_text("int value = 1;\n" * 65536, encoding="utf-8")
        process = subprocess.Popen([str(args.nsc3), "--dump-tokens", str(large)], stdout=subprocess.DEVNULL,
                                   stderr=subprocess.DEVNULL)
        time.sleep(0.01)
        interrupted = process.poll() is None
        if interrupted: process.terminate()
        process.wait(timeout=5)

        unexpected_temps = [p.name for p in args.output_dir.iterdir() if p.suffix.lower() in {".tmp", ".temp"}]
        require(not unexpected_temps, f"temporary files leaked: {unexpected_temps}")
        report = {
            "schema": "nativesharp-selfhost-sh13-report", "version": 1, "status": "local-passed",
            "fuzz_cases": len(fuzz_results), "fuzz": fuzz_results,
            "corrupt_metadata_rejected": True, "atomic_output_failure": True,
            "interruption_exercised": interrupted, "temporary_leaks": unexpected_temps,
            "startup_seconds": timings, "startup_ratio": startup_ratio,
            "compile_seconds": compile_times, "binary_size_ratio": size_ratio,
            "side_by_side": {"nsc0": str(args.nsc0), "nsc2": str(args.nsc2), "nsc3": str(args.nsc3)},
            "artifacts": artifacts, "work_items": contract["work_items"],
            "inherited_blockers": ["SH12 second-machine attestation", "source-derived compiler codegen"],
            "problems": [],
        }
        args.report.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    except (OSError, json.JSONDecodeError, Error, subprocess.TimeoutExpired) as exc:
        print(f"verify_robustness: error: {exc}", file=sys.stderr); return 1
    print("SH13 local robustness and performance budgets passed")
    return 0


if __name__ == "__main__": raise SystemExit(main())
