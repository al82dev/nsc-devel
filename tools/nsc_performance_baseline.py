#!/usr/bin/env python3
"""Reproducible NativeSharp compiler performance baseline.

The runner intentionally uses only the Python standard library. It measures NSC as an external
process, parses the compiler's own ``--time`` phase report, samples peak working set on Windows,
records output size, and executes produced programs when the corpus declares an expected exit.
It never edits source projects; every artifact and log is written below the selected output root.
"""

from __future__ import annotations

import argparse
import ctypes
import hashlib
import json
import math
import os
import platform
import random
import re
import statistics
import subprocess
import sys
import time
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Iterable


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_NATIVE_SHARP_ROOT = Path(os.environ.get("NS_ROOT", r"C:\native-sharp"))
DEFAULT_COMPILER = Path(os.environ.get("NSC_EXE", DEFAULT_NATIVE_SHARP_ROOT / "bin" / "nsc.exe"))
DEFAULT_LLVM = Path(os.environ.get("NSC_LLVM_ROOT", r"C:\clang-llvm"))
DEFAULT_NSLINK = Path(os.environ.get("NSLINK_EXE", DEFAULT_NATIVE_SHARP_ROOT / "bin" / "nslink.exe"))
DEFAULT_CORPUS = ROOT / "tests" / "performance" / "p00" / "corpus.json"
DEFAULT_OUTPUT = ROOT / "_nscbuilder" / "performance" / "p00"
TIME_RE = re.compile(r"^timing phase=(\S+) us=(\d+)$", re.MULTILINE)
TOTAL_RE = re.compile(r"^timing total_ms=(\d+)$", re.MULTILINE)


@dataclass(frozen=True)
class Corpus:
    name: str
    project: str
    expected_exit: int | None
    tags: tuple[str, ...]


@dataclass(frozen=True)
class Job:
    corpus: Corpus
    artifact: str
    optimization: str
    debug: str
    linker: str
    iteration: int
    warmup: bool


@dataclass
class Measurement:
    corpus: str
    project: str
    artifact: str
    optimization: str
    debug: str
    linker: str
    iteration: int
    warmup: bool
    command: list[str]
    exit_code: int
    wall_ms: float
    compiler_total_ms: int | None
    peak_working_set_bytes: int | None
    artifact_bytes: int | None
    runtime_ms: float | None
    runtime_exit: int | None
    expected_exit: int | None
    phases_us: dict[str, int]
    log: str
    status: str


class ProcessMemoryCounters(ctypes.Structure):
    _fields_ = [
        ("cb", ctypes.c_ulong),
        ("PageFaultCount", ctypes.c_ulong),
        ("PeakWorkingSetSize", ctypes.c_size_t),
        ("WorkingSetSize", ctypes.c_size_t),
        ("QuotaPeakPagedPoolUsage", ctypes.c_size_t),
        ("QuotaPagedPoolUsage", ctypes.c_size_t),
        ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t),
        ("QuotaNonPagedPoolUsage", ctypes.c_size_t),
        ("PagefileUsage", ctypes.c_size_t),
        ("PeakPagefileUsage", ctypes.c_size_t),
    ]


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def percentile(values: Iterable[float], fraction: float) -> float:
    ordered = sorted(values)
    if not ordered:
        return 0.0
    return ordered[max(0, math.ceil(len(ordered) * fraction) - 1)]


def sample_peak_working_set(process: subprocess.Popen[str]) -> int | None:
    if os.name != "nt" or not hasattr(process, "_handle"):
        return None
    counters = ProcessMemoryCounters()
    counters.cb = ctypes.sizeof(counters)
    ok = ctypes.windll.psapi.GetProcessMemoryInfo(  # type: ignore[attr-defined]
        int(process._handle), ctypes.byref(counters), counters.cb  # type: ignore[attr-defined]
    )
    return int(counters.PeakWorkingSetSize) if ok else None


def run_process(command: list[str], cwd: Path, env: dict[str, str]) -> tuple[int, str, float, int | None]:
    started = time.perf_counter()
    process = subprocess.Popen(
        command,
        cwd=cwd,
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    peak: int | None = None
    while process.poll() is None:
        sample = sample_peak_working_set(process)
        if sample is not None:
            peak = sample if peak is None else max(peak, sample)
        time.sleep(0.005)
    output, _ = process.communicate()
    sample = sample_peak_working_set(process)
    if sample is not None:
        peak = sample if peak is None else max(peak, sample)
    elapsed_ms = (time.perf_counter() - started) * 1000.0
    return process.returncode, output, elapsed_ms, peak


def load_corpora(path: Path) -> list[Corpus]:
    document = json.loads(path.read_text(encoding="utf-8"))
    if document.get("schema") != "nsc-performance-corpus-1":
        raise ValueError(f"unsupported corpus schema in {path}")
    result = []
    for row in document.get("corpora", []):
        corpus = Corpus(
            name=str(row["name"]),
            project=str(row["project"]),
            expected_exit=row.get("expected_exit"),
            tags=tuple(str(tag) for tag in row.get("tags", [])),
        )
        project = (ROOT / corpus.project).resolve()
        if not project.is_file():
            raise FileNotFoundError(f"missing corpus project: {project}")
        result.append(corpus)
    if not result:
        raise ValueError("the performance corpus is empty")
    return result


def make_environment(
    linker: str,
    compiler: Path,
    llvm_root: Path,
    nslink: Path,
) -> dict[str, str]:
    env = dict(os.environ)
    native_sharp_root = compiler.resolve().parent.parent
    env["NS_ROOT"] = str(native_sharp_root)
    env["NS_LIB_ROOT"] = str(native_sharp_root / "lib")
    env["NS_LIB_READY"] = "1"
    env["NSC_TEST_NONINTERACTIVE"] = "1"
    env["CTEST_INTERACTIVE_DEBUG_MODE"] = "0"
    env["NSC_LLVM_LIB"] = str(llvm_root / "bin" / "llvm-lib.exe")
    env["NSC_LLD_LINK"] = str(
        llvm_root / "bin" / "lld-link.exe" if linker == "lld-link" else nslink
    )
    env.pop("NSC_KEEP_LINK_TEMPS", None)
    return env


def output_extension(artifact: str) -> str:
    return ".obj" if artifact == "obj" else ".exe"


def compile_job(
    job: Job,
    compiler: Path,
    llvm_root: Path,
    nslink: Path,
    run_dir: Path,
) -> Measurement:
    safe_debug = job.debug.replace("-", "")
    stem = (
        f"{job.corpus.name}_{job.artifact}_{job.optimization}_{safe_debug}_"
        f"{job.linker}_{'warmup' if job.warmup else job.iteration}"
    )
    artifact = run_dir / "artifacts" / (stem + output_extension(job.artifact))
    log = run_dir / "logs" / (stem + ".log")
    artifact.parent.mkdir(parents=True, exist_ok=True)
    log.parent.mkdir(parents=True, exist_ok=True)
    project = (ROOT / job.corpus.project).resolve()
    action = "--emit-project-oop-obj" if job.artifact == "obj" else "--emit-project-oop-exe"
    command = [
        str(compiler),
        action,
        str(project),
        f"-{job.optimization}",
        job.debug,
        "--time",
        "-o",
        str(artifact),
    ]
    linker = job.linker if job.artifact == "exe" else "none"
    env = make_environment(job.linker, compiler, llvm_root, nslink)
    exit_code, output, wall_ms, peak = run_process(command, ROOT, env)
    log.write_text(output, encoding="utf-8")
    phases = {name: int(value) for name, value in TIME_RE.findall(output)}
    total_match = TOTAL_RE.search(output)
    total_ms = int(total_match.group(1)) if total_match else None
    artifact_bytes = artifact.stat().st_size if artifact.is_file() else None
    runtime_ms: float | None = None
    runtime_exit: int | None = None
    if exit_code == 0 and job.artifact == "exe" and job.corpus.expected_exit is not None:
        runtime_started = time.perf_counter()
        execution = subprocess.run(
            [str(artifact)], cwd=artifact.parent, env=env, check=False,
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
        runtime_ms = (time.perf_counter() - runtime_started) * 1000.0
        runtime_exit = execution.returncode
    expected_ok = (
        job.corpus.expected_exit is None
        or job.artifact == "obj"
        or runtime_exit == job.corpus.expected_exit
    )
    status = "passed" if exit_code == 0 and artifact_bytes is not None and expected_ok else "failed"
    return Measurement(
        corpus=job.corpus.name,
        project=job.corpus.project,
        artifact=job.artifact,
        optimization=job.optimization,
        debug=job.debug,
        linker=linker,
        iteration=job.iteration,
        warmup=job.warmup,
        command=command,
        exit_code=exit_code,
        wall_ms=round(wall_ms, 3),
        compiler_total_ms=total_ms,
        peak_working_set_bytes=peak,
        artifact_bytes=artifact_bytes,
        runtime_ms=round(runtime_ms, 3) if runtime_ms is not None else None,
        runtime_exit=runtime_exit,
        expected_exit=job.corpus.expected_exit,
        phases_us=phases,
        log=str(log.relative_to(run_dir)),
        status=status,
    )


def summary_rows(measurements: list[Measurement]) -> list[dict[str, object]]:
    groups: dict[tuple[str, str, str, str, str], list[Measurement]] = {}
    for row in measurements:
        if row.warmup:
            continue
        key = (row.corpus, row.artifact, row.optimization, row.debug, row.linker)
        groups.setdefault(key, []).append(row)
    summaries = []
    for key in sorted(groups):
        rows = groups[key]
        wall = [row.wall_ms for row in rows]
        memory = [row.peak_working_set_bytes for row in rows if row.peak_working_set_bytes]
        sizes = [row.artifact_bytes for row in rows if row.artifact_bytes is not None]
        runtime = [row.runtime_ms for row in rows if row.runtime_ms is not None]
        phase_names = sorted({name for row in rows for name in row.phases_us})
        phase_medians = {
            name: round(statistics.median([row.phases_us.get(name, 0) for row in rows]))
            for name in phase_names
        }
        summaries.append({
            "corpus": key[0],
            "artifact": key[1],
            "optimization": key[2],
            "debug": key[3],
            "linker": key[4],
            "samples": len(rows),
            "passed": sum(row.status == "passed" for row in rows),
            "wall_median_ms": round(statistics.median(wall), 3),
            "wall_p95_ms": round(percentile(wall, 0.95), 3),
            "peak_working_set_median_bytes": round(statistics.median(memory)) if memory else None,
            "artifact_median_bytes": round(statistics.median(sizes)) if sizes else None,
            "runtime_median_ms": round(statistics.median(runtime), 3) if runtime else None,
            "phases_median_us": phase_medians,
        })
    return summaries


def aggregate_phase_rows(measurements: list[Measurement]) -> list[dict[str, object]]:
    phase_values: dict[str, list[int]] = {}
    for row in measurements:
        if row.warmup or row.status != "passed":
            continue
        for name, elapsed_us in row.phases_us.items():
            phase_values.setdefault(name, []).append(elapsed_us)
    rows = [
        {
            "phase": name,
            "samples": len(values),
            "median_us": round(statistics.median(values)),
            "p95_us": round(percentile(values, 0.95)),
        }
        for name, values in phase_values.items()
    ]
    return sorted(rows, key=lambda row: int(row["median_us"]), reverse=True)


def write_markdown(
    path: Path,
    metadata: dict[str, object],
    summaries: list[dict[str, object]],
    phases: list[dict[str, object]],
) -> None:
    lines = [
        "# NSC performance baseline",
        "",
        f"- Run: `{metadata['run_id']}`",
        f"- Compiler: `{metadata['compiler_version']}`",
        f"- Compiler SHA-256: `{metadata['compiler_sha256']}`",
        f"- Iterations: {metadata['iterations']} measured + {metadata['warmups']} warmup",
        f"- Failures: {metadata['failures']}",
        "",
        "| Corpus | Artifact | Opt | Debug | Linker | Median ms | p95 ms | Peak MiB | Size KiB | Run ms |",
        "|---|---|---:|---|---|---:|---:|---:|---:|---:|",
    ]
    for row in summaries:
        peak = row["peak_working_set_median_bytes"]
        size = row["artifact_median_bytes"]
        runtime = row["runtime_median_ms"]
        lines.append(
            f"| {row['corpus']} | {row['artifact']} | {row['optimization']} | "
            f"{row['debug']} | {row['linker']} | {row['wall_median_ms']} | "
            f"{row['wall_p95_ms']} | "
            f"{round(int(peak) / 1048576, 2) if peak else '-'} | "
            f"{round(int(size) / 1024, 2) if size else '-'} | {runtime if runtime is not None else '-'} |"
        )
    lines.extend([
        "",
        "## Five largest compiler phases",
        "",
        "These values aggregate all successful measured jobs; warmups are excluded.",
        "",
        "| Phase | Samples | Median ms | p95 ms |",
        "|---|---:|---:|---:|",
    ])
    for row in phases[:5]:
        lines.append(
            f"| {row['phase']} | {row['samples']} | "
            f"{round(int(row['median_us']) / 1000, 3)} | "
            f"{round(int(row['p95_us']) / 1000, 3)} |"
        )
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def split_values(text: str) -> list[str]:
    return [value.strip() for value in text.split(",") if value.strip()]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, default=DEFAULT_COMPILER)
    parser.add_argument("--llvm-root", type=Path, default=DEFAULT_LLVM)
    parser.add_argument("--nslink", type=Path, default=DEFAULT_NSLINK)
    parser.add_argument("--corpus-file", type=Path, default=DEFAULT_CORPUS)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--corpus", default="all", help="comma-separated names or all")
    parser.add_argument("--levels", default="O0,O1,O2,O3")
    parser.add_argument("--debug", default="-g0,-gline-tables-only,-g")
    parser.add_argument("--artifacts", default="obj,exe")
    parser.add_argument("--linkers", default="lld-link,NSLink")
    parser.add_argument("--iterations", type=int, default=5)
    parser.add_argument("--warmups", type=int, default=1)
    parser.add_argument("--seed", type=int, default=200)
    args = parser.parse_args()

    compiler = args.compiler.resolve()
    llvm_root = args.llvm_root.resolve()
    nslink = args.nslink.resolve()
    if not compiler.is_file():
        parser.error(f"compiler not found: {compiler}")
    if not (llvm_root / "bin" / "lld-link.exe").is_file():
        parser.error(f"lld-link not found below: {llvm_root}")
    if not nslink.is_file():
        parser.error(f"NSLink not found: {nslink}")
    if args.iterations < 1 or args.warmups < 0:
        parser.error("iterations must be positive and warmups cannot be negative")

    corpora = load_corpora(args.corpus_file.resolve())
    selected = split_values(args.corpus)
    if selected != ["all"]:
        wanted = set(selected)
        corpora = [corpus for corpus in corpora if corpus.name in wanted]
        missing = wanted - {corpus.name for corpus in corpora}
        if missing:
            parser.error("unknown corpus: " + ", ".join(sorted(missing)))
    levels = split_values(args.levels)
    debug_modes = split_values(args.debug)
    artifacts = split_values(args.artifacts)
    linkers = split_values(args.linkers)
    if any(level not in {"O0", "O1", "O2", "O3"} for level in levels):
        parser.error("levels must be O0, O1, O2, or O3")
    if any(mode not in {"-g0", "-gline-tables-only", "-g"} for mode in debug_modes):
        parser.error("debug modes must be -g0, -gline-tables-only, or -g")
    if any(kind not in {"obj", "exe"} for kind in artifacts):
        parser.error("artifacts must be obj or exe")
    if any(linker not in {"lld-link", "NSLink"} for linker in linkers):
        parser.error("linkers must be lld-link or NSLink")

    stamp = datetime.now().astimezone().strftime("%Y%m%d-%H%M%S")
    run_id = f"{stamp}-{random.Random(args.seed).getrandbits(32):08x}"
    run_dir = args.output.resolve() / run_id
    run_dir.mkdir(parents=True, exist_ok=False)
    version = subprocess.run(
        [str(compiler), "--version"], check=True, capture_output=True, text=True,
        encoding="utf-8", errors="replace",
    ).stdout.strip().replace("\n", " | ")

    warmup_jobs: list[Job] = []
    measured_jobs: list[Job] = []
    for corpus in corpora:
        for artifact in artifacts:
            effective_linkers = linkers if artifact == "exe" else [linkers[0]]
            for level in levels:
                for debug in debug_modes:
                    for linker in effective_linkers:
                        for iteration in range(args.warmups):
                            warmup_jobs.append(Job(corpus, artifact, level, debug, linker, iteration, True))
                        for iteration in range(args.iterations):
                            measured_jobs.append(Job(corpus, artifact, level, debug, linker, iteration, False))
    random.Random(args.seed).shuffle(measured_jobs)

    measurements: list[Measurement] = []
    total = len(warmup_jobs) + len(measured_jobs)
    for index, job in enumerate([*warmup_jobs, *measured_jobs], 1):
        row = compile_job(job, compiler, llvm_root, nslink, run_dir)
        measurements.append(row)
        marker = "warmup" if job.warmup else f"sample {job.iteration + 1}"
        print(
            f"[{index}/{total}] {row.status.upper()} {job.corpus.name} {job.artifact} "
            f"{job.optimization} {job.debug} {row.linker} {marker}: {row.wall_ms:.3f} ms",
            flush=True,
        )

    summaries = summary_rows(measurements)
    phases = aggregate_phase_rows(measurements)
    failures = sum(row.status != "passed" for row in measurements)
    metadata: dict[str, object] = {
        "schema": "nsc-performance-baseline-1",
        "run_id": run_id,
        "recorded_at": datetime.now(timezone.utc).isoformat(),
        "root": str(ROOT),
        "compiler": str(compiler),
        "compiler_version": version,
        "compiler_sha256": sha256_file(compiler),
        "llvm_root": str(llvm_root),
        "nslink": str(nslink),
        "nslink_sha256": sha256_file(nslink),
        "machine": {
            "platform": platform.platform(),
            "processor": platform.processor(),
            "python": platform.python_version(),
            "logical_cpus": os.cpu_count(),
        },
        "iterations": args.iterations,
        "warmups": args.warmups,
        "seed": args.seed,
        "failures": failures,
    }
    (run_dir / "measurements.json").write_text(
        json.dumps({"metadata": metadata, "measurements": [asdict(row) for row in measurements],
                    "summary": summaries, "phases": phases}, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    write_markdown(run_dir / "report.md", metadata, summaries, phases)
    print(f"Report: {run_dir / 'report.md'}")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
