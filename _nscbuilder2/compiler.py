"""Separated compiler build plans and persistent NSC artifact receipts."""

from __future__ import annotations

from dataclasses import asdict, dataclass
from datetime import datetime, timezone
from pathlib import Path

from .model import RunStep
from .toolchain import ArtifactEvidence, sha256_file


def _a_path(value: str | Path, label: str) -> Path:
    """Resolve a compiler work path and enforce the A:-only build boundary."""

    path = Path(value).resolve()
    if path.drive.upper() != "A:":
        raise ValueError(f"{label} must stay on A: {path}")
    return path


@dataclass(frozen=True)
class CompilerBuildOptions:
    """Represent all choices that can affect the produced nsc.exe."""

    source_root: Path
    build_dir: Path
    nslink: Path
    suite_prefix: Path
    configuration: str = "Release"
    linkage: str = "STATIC"
    host_optimization: str = "O2"
    runner: str = "cmake"
    clean: bool = False
    jobs: int = 12

    def validated(self) -> "CompilerBuildOptions":
        """Reject implicit tools, invalid profiles, and build paths outside A:."""

        source = _a_path(self.source_root, "compiler source root")
        build = _a_path(self.build_dir, "compiler build directory")
        nslink = _a_path(self.nslink, "compiler NSLink")
        prefix = _a_path(self.suite_prefix, "test suite prefix")
        if not (source / "CMakeLists.txt").is_file():
            raise ValueError(f"compiler source root has no CMakeLists.txt: {source}")
        if not nslink.is_file():
            raise ValueError(f"compiler NSLink does not exist: {nslink}")
        if self.configuration not in {"Release", "Debug"}:
            raise ValueError("compiler configuration must be Release or Debug")
        if self.linkage not in {"STATIC", "DYNAMIC"}:
            raise ValueError("compiler linkage must be STATIC or DYNAMIC")
        if self.host_optimization not in {"O0", "O1", "O2", "O3"}:
            raise ValueError("host optimization must be O0, O1, O2, or O3")
        if self.runner not in {"cmake", "ninja"}:
            raise ValueError("compiler build runner must be cmake or ninja")
        if self.jobs < 1:
            raise ValueError("compiler jobs must be a positive integer")
        return CompilerBuildOptions(
            source,
            build,
            nslink,
            prefix,
            self.configuration,
            self.linkage,
            self.host_optimization,
            self.runner,
            self.clean,
            self.jobs,
        )

    def to_dict(self) -> dict[str, object]:
        """Serialize every build identity field for evidence and comparison."""

        value = asdict(self)
        for key in ("source_root", "build_dir", "nslink", "suite_prefix"):
            value[key] = str(value[key])
        return value


@dataclass(frozen=True)
class CompilerBuildPlan:
    """Expose configure/build as an operation independent from test execution."""

    run_id: str
    options: CompilerBuildOptions
    history_dir: Path
    work_dir: Path
    compiler: Path
    steps: tuple[RunStep, ...]

    def to_dict(self) -> dict[str, object]:
        """Serialize the complete compiler build command sequence."""

        return {
            "kind": "compiler-build",
            "run_id": self.run_id,
            "compiler_build": self.options.to_dict(),
            "history_dir": str(self.history_dir),
            "work_dir": str(self.work_dir),
            "compiler": str(self.compiler),
            "steps": [step.to_dict() for step in self.steps],
        }


@dataclass(frozen=True)
class CompilerReceipt:
    """Freeze the NSC artifact and all options required to reuse its test tree."""

    options: CompilerBuildOptions
    created_at: str
    artifact: ArtifactEvidence

    def to_dict(self) -> dict[str, object]:
        """Return a stable JSON-compatible compiler receipt."""

        return {
            "schema": 1,
            "options": self.options.to_dict(),
            "created_at": self.created_at,
            "artifact": asdict(self.artifact),
        }

    @staticmethod
    def from_dict(value: dict[str, object]) -> "CompilerReceipt":
        """Load a receipt without accepting missing nested objects."""

        raw_options = value.get("options")
        raw_artifact = value.get("artifact")
        if not isinstance(raw_options, dict) or not isinstance(raw_artifact, dict):
            raise ValueError("invalid compiler receipt")
        options = CompilerBuildOptions(
            Path(str(raw_options["source_root"])),
            Path(str(raw_options["build_dir"])),
            Path(str(raw_options["nslink"])),
            Path(str(raw_options["suite_prefix"])),
            str(raw_options["configuration"]),
            str(raw_options["linkage"]),
            str(raw_options["host_optimization"]),
            str(raw_options["runner"]),
            bool(raw_options["clean"]),
            int(raw_options["jobs"]),
        )
        return CompilerReceipt(options, str(value["created_at"]), ArtifactEvidence(**raw_artifact))


def create_compiler_build_plan(
    options: CompilerBuildOptions,
    run_id: str,
    history_dir: Path,
    work_dir: Path,
) -> CompilerBuildPlan:
    """Create an NSC-only build; tests remain configured but are not built or run."""

    selected = options.validated()
    history = _a_path(history_dir, "compiler history directory")
    work = _a_path(work_dir, "compiler work directory")
    compiler = selected.build_dir / "bin" / "nsc.exe"
    configure = [
        "cmake",
        "--fresh" if selected.clean else "-S",
    ]
    if selected.clean:
        configure.extend(("-S", str(selected.source_root)))
    else:
        configure.append(str(selected.source_root))
    configure.extend(
        (
            "-B",
            str(selected.build_dir),
            "-G",
            "Ninja",
            f"-DCMAKE_BUILD_TYPE={selected.configuration}",
            "-DNSC_BUILD_TESTS=ON",
            "-DNSC_WARNINGS_AS_ERRORS=ON",
            f"-DNSC_HOST_LINKAGE={selected.linkage}",
            f"-DNSC_HOST_OPTIMIZATION={selected.host_optimization}",
            "-DNSC_TEST_INFRASTRUCTURE_V2=ON",
            f"-DNSC_TEST_SUITE_PREFIX={selected.suite_prefix.as_posix()}",
            f"-DNSC_TEST_JOBS={selected.jobs}",
            f"-DNSC_NSLINK_EXECUTABLE={selected.nslink.as_posix()}",
        )
    )
    if selected.runner == "cmake":
        build = (
            "cmake",
            "--build",
            str(selected.build_dir),
            "--target",
            "nsc",
            "--parallel",
            str(selected.jobs),
            "--",
            "-k",
            "0",
        )
    else:
        build = ("ninja", "-C", str(selected.build_dir), "-j", str(selected.jobs), "-k", "0", "nsc")
    steps = (
        RunStep("compiler-configure", "Configure NSC compiler", tuple(configure), selected.source_root),
        RunStep(
            "compiler-build",
            "Build NSC compiler only",
            build,
            selected.source_root,
            freezes_compiler=True,
        ),
    )
    return CompilerBuildPlan(run_id, selected, history, work, compiler, steps)


def create_compiler_receipt(plan: CompilerBuildPlan, status: str) -> CompilerReceipt:
    """Issue the reusable compiler receipt only after a successful NSC build."""

    if status != "passed":
        raise ValueError(f"cannot issue compiler receipt: run status is {status}")
    return CompilerReceipt(
        plan.options,
        datetime.now(timezone.utc).isoformat(),
        ArtifactEvidence.capture(plan.compiler),
    )


def validate_compiler_receipt(receipt: CompilerReceipt) -> Path:
    """Return the current nsc.exe only when its identity still matches."""

    artifact = receipt.artifact
    path = Path(artifact.path).resolve()
    if not path.is_file():
        raise ValueError(f"compiler receipt artifact is missing: {path}")
    stat = path.stat()
    if stat.st_size != artifact.size or stat.st_mtime_ns != artifact.modified_ns:
        raise ValueError(f"compiler receipt artifact changed: {path}")
    if sha256_file(path) != artifact.sha256:
        raise ValueError(f"compiler receipt artifact hash changed: {path}")
    return path
