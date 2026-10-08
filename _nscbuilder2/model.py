"""Immutable configuration and command planning for NSC Builder 2."""

from __future__ import annotations

import os
from dataclasses import asdict, dataclass
from pathlib import Path


def _a_path(value: str | Path, label: str) -> Path:
    path = Path(value).resolve()
    if path.drive.upper() != "A:":
        raise ValueError(f"{label} must stay on A: {path}")
    return path


@dataclass(frozen=True)
class BuilderConfig:
    source_root: Path
    build_dir: Path
    nslink: Path
    suite_prefix: Path
    jobs: int = 12
    build_type: str = "Release"

    @staticmethod
    def defaults(source_root: Path) -> "BuilderConfig":
        root = source_root.resolve()
        return BuilderConfig(
            source_root=root,
            build_dir=root / "build-test-v2",
            nslink=Path("A:/nslink-devel/bin/release/nslink.exe"),
            suite_prefix=Path("A:/native-sharp-3.0-baseline"),
            jobs=max(1, min(12, os.cpu_count() or 1)),
        )

    @staticmethod
    def from_dict(value: dict[str, object]) -> "BuilderConfig":
        return BuilderConfig(
            source_root=Path(str(value["source_root"])),
            build_dir=Path(str(value["build_dir"])),
            nslink=Path(str(value["nslink"])),
            suite_prefix=Path(str(value["suite_prefix"])),
            jobs=int(value.get("jobs", 12)),
            build_type=str(value.get("build_type", "Release")),
        )

    def to_dict(self) -> dict[str, object]:
        value = asdict(self)
        for key in ("source_root", "build_dir", "nslink", "suite_prefix"):
            value[key] = str(value[key])
        return value

    def validated(self) -> "BuilderConfig":
        source = _a_path(self.source_root, "source root")
        build = _a_path(self.build_dir, "build directory")
        nslink = _a_path(self.nslink, "NSLink")
        prefix = _a_path(self.suite_prefix, "suite prefix")
        if not (source / "CMakeLists.txt").is_file():
            raise ValueError(f"source root has no CMakeLists.txt: {source}")
        if not nslink.is_file():
            raise ValueError(f"NSLink does not exist: {nslink}")
        if not (prefix / "bin").is_dir() or not (prefix / "lib").is_dir():
            raise ValueError(f"suite prefix must contain bin and lib: {prefix}")
        if self.jobs < 1:
            raise ValueError("jobs must be a positive integer")
        if self.build_type not in {"Release", "Debug"}:
            raise ValueError("build type must be Release or Debug")
        return BuilderConfig(source, build, nslink, prefix, self.jobs, self.build_type)


@dataclass(frozen=True)
class RunSelection:
    unit: bool = True
    manifest_mode: str = "all"
    manifest_value: str = ""
    specialized: bool = True

    def validated(self) -> "RunSelection":
        if self.manifest_mode not in {"none", "all", "category", "case"}:
            raise ValueError(f"unsupported manifest mode: {self.manifest_mode}")
        value = self.manifest_value.strip()
        if self.manifest_mode in {"category", "case"} and not value:
            raise ValueError(f"manifest {self.manifest_mode} requires a value")
        if not self.unit and self.manifest_mode == "none" and not self.specialized:
            raise ValueError("select at least one test layer")
        return RunSelection(self.unit, self.manifest_mode, value, self.specialized)

    def to_dict(self) -> dict[str, object]:
        return asdict(self)


@dataclass(frozen=True)
class RunStep:
    identifier: str
    title: str
    command: tuple[str, ...]
    cwd: Path
    freezes_compiler: bool = False

    def to_dict(self) -> dict[str, object]:
        return {
            "identifier": self.identifier,
            "title": self.title,
            "command": list(self.command),
            "cwd": str(self.cwd),
            "freezes_compiler": self.freezes_compiler,
        }


@dataclass(frozen=True)
class RunPlan:
    run_id: str
    config: BuilderConfig
    selection: RunSelection
    history_dir: Path
    work_dir: Path
    compiler: Path
    steps: tuple[RunStep, ...]

    def to_dict(self) -> dict[str, object]:
        return {
            "run_id": self.run_id,
            "config": self.config.to_dict(),
            "selection": self.selection.to_dict(),
            "history_dir": str(self.history_dir),
            "work_dir": str(self.work_dir),
            "compiler": str(self.compiler),
            "steps": [step.to_dict() for step in self.steps],
        }


def create_run_plan(
    config: BuilderConfig,
    selection: RunSelection,
    run_id: str,
    history_dir: Path,
    work_dir: Path,
) -> RunPlan:
    """Create the complete, deterministic Builder 2 command sequence."""

    cfg = config.validated()
    chosen = selection.validated()
    history = _a_path(history_dir, "history directory")
    work = _a_path(work_dir, "work directory")
    compiler = cfg.build_dir / "bin" / "nsc.exe"
    runtime = cfg.build_dir / "generated" / "NativeSharp.Runtime.Win32.lib"
    runner = cfg.source_root / "tests" / "harness" / "run_nsc_tests.py"
    results = history / "harness-results"
    harness_work = work / "harness-work"

    configure = (
        "cmake",
        "-S",
        str(cfg.source_root),
        "-B",
        str(cfg.build_dir),
        "-G",
        "Ninja",
        f"-DCMAKE_BUILD_TYPE={cfg.build_type}",
        "-DNSC_BUILD_TESTS=ON",
        "-DNSC_WARNINGS_AS_ERRORS=ON",
        "-DNSC_HOST_LINKAGE=STATIC",
        "-DNSC_HOST_OPTIMIZATION=O2",
        "-DNSC_TEST_INFRASTRUCTURE_V2=ON",
        f"-DNSC_TEST_SUITE_PREFIX={cfg.suite_prefix.as_posix()}",
        f"-DNSC_TEST_JOBS={cfg.jobs}",
        f"-DNSC_NSLINK_EXECUTABLE={cfg.nslink.as_posix()}",
    )
    build = (
        "cmake",
        "--build",
        str(cfg.build_dir),
        "--target",
        "pruebas",
        "--parallel",
        str(cfg.jobs),
        "--",
        "-k",
        "0",
    )
    steps: list[RunStep] = [
        RunStep("configure", "Configure compiler and v2 tests", configure, cfg.source_root),
        RunStep("build", "Build compiler once", build, cfg.source_root, freezes_compiler=True),
    ]

    ctest_prefix = (
        "ctest",
        "--test-dir",
        str(cfg.build_dir),
        "--interactive-debug-mode",
        "0",
        "--output-on-failure",
    )
    if chosen.unit:
        steps.append(
            RunStep(
                "unit",
                "Run consolidated C++ unit registry",
                (*ctest_prefix, "-R", "^nsc\\.unit\\.consolidated$"),
                cfg.source_root,
            )
        )
    if chosen.manifest_mode == "all":
        steps.append(
            RunStep(
                "manifests",
                "Run all NativeSharp O0 manifests",
                (*ctest_prefix, "-R", "^nsc\\.harness\\.cases-o0$"),
                cfg.source_root,
            )
        )
    elif chosen.manifest_mode in {"category", "case"}:
        selector = "--component" if chosen.manifest_mode == "category" else "--case"
        steps.append(
            RunStep(
                "manifests",
                f"Run NativeSharp O0 {chosen.manifest_mode}",
                (
                    "python",
                    "-B",
                    str(runner),
                    "--nsc",
                    str(compiler),
                    "--nslink",
                    str(cfg.nslink),
                    "--library-root",
                    str(cfg.suite_prefix / "lib"),
                    "--runtime-library",
                    str(runtime),
                    "--optimization",
                    "O0",
                    "--jobs",
                    str(cfg.jobs),
                    "--work-root",
                    str(harness_work),
                    "--results-root",
                    str(results),
                    "--run-id",
                    run_id,
                    selector,
                    chosen.manifest_value,
                ),
                cfg.source_root,
            )
        )
    if chosen.specialized:
        steps.append(
            RunStep(
                "specialized",
                "Run specialized O0 suites",
                (*ctest_prefix, "-R", "^nsc\\.harness\\.specialized-o0$"),
                cfg.source_root,
            )
        )
    return RunPlan(run_id, cfg, chosen, history, work, compiler, tuple(steps))
