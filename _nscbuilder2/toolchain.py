"""Tool producer plans, receipts, and explicit A:-only staging."""

from __future__ import annotations

import hashlib
import json
import os
import shutil
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
from pathlib import Path

from .model import RunStep


TOOL_NAMES = ("nslink", "nsasm", "nsdbg")


def _a_path(value: str | Path, label: str) -> Path:
    """Resolve one Builder 2 work path and reject destinations outside A:."""

    path = Path(value).resolve()
    if path.drive.upper() != "A:":
        raise ValueError(f"{label} must stay on A: {path}")
    return path


def sha256_file(path: Path) -> str:
    """Hash one artifact iteratively with constant auxiliary memory."""

    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while block := stream.read(1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


@dataclass(frozen=True)
class ToolDefinition:
    """Describe the complete product set emitted by one external producer."""

    name: str
    targets: tuple[str, ...]
    required_products: tuple[str, ...]
    optional_products: tuple[str, ...] = ()


DEFINITIONS = {
    "nslink": ToolDefinition("nslink", ("nslink",), ("nslink.exe",)),
    "nsasm": ToolDefinition(
        "nsasm",
        ("nsasm", "nsdump", "nsdis"),
        ("nsasm.exe", "nsdump.exe", "nsdis.exe"),
    ),
    "nsdbg": ToolDefinition("nsdbg", ("nsdbg",), ("nsdbg.exe",), ("nsdbg.pdb",)),
}


@dataclass(frozen=True)
class ToolBuildOptions:
    """Hold an explicit build boundary for one tool producer."""

    name: str
    source_root: Path
    build_dir: Path
    output_dir: Path
    configuration: str = "Release"
    jobs: int = 12

    def validated(self) -> "ToolBuildOptions":
        """Validate producer identity, Windows configuration, and A: paths."""

        name = self.name.casefold()
        if name not in DEFINITIONS:
            raise ValueError(f"unsupported tool producer: {self.name}")
        source = _a_path(self.source_root, f"{name} source root")
        build = _a_path(self.build_dir, f"{name} build directory")
        output = _a_path(self.output_dir, f"{name} output directory")
        if not (source / "CMakeLists.txt").is_file():
            raise ValueError(f"{name} source root has no CMakeLists.txt: {source}")
        if self.configuration not in {"Release", "Debug"}:
            raise ValueError("tool configuration must be Release or Debug")
        if self.jobs < 1:
            raise ValueError("tool jobs must be a positive integer")
        return ToolBuildOptions(name, source, build, output, self.configuration, self.jobs)

    def to_dict(self) -> dict[str, object]:
        """Serialize the options without relying on process-global state."""

        value = asdict(self)
        for key in ("source_root", "build_dir", "output_dir"):
            value[key] = str(value[key])
        return value


@dataclass(frozen=True)
class ToolBuildPlan:
    """Expose a tool build through the existing sequential pipeline contract."""

    run_id: str
    options: ToolBuildOptions
    history_dir: Path
    work_dir: Path
    compiler: Path
    products: tuple[Path, ...]
    optional_products: tuple[Path, ...]
    steps: tuple[RunStep, ...]

    def to_dict(self) -> dict[str, object]:
        """Serialize commands and the complete expected product inventory."""

        return {
            "kind": "tool-build",
            "run_id": self.run_id,
            "tool": self.options.to_dict(),
            "history_dir": str(self.history_dir),
            "work_dir": str(self.work_dir),
            "compiler": str(self.compiler),
            "products": [str(path) for path in self.products],
            "optional_products": [str(path) for path in self.optional_products],
            "steps": [step.to_dict() for step in self.steps],
        }


@dataclass(frozen=True)
class ArtifactEvidence:
    """Freeze identity and mutable file attributes for one product."""

    path: str
    sha256: str
    size: int
    modified_ns: int

    @staticmethod
    def capture(path: Path) -> "ArtifactEvidence":
        """Capture evidence only after the producer emitted the product."""

        resolved = path.resolve()
        if not resolved.is_file():
            raise ValueError(f"tool product does not exist: {resolved}")
        stat = resolved.stat()
        return ArtifactEvidence(str(resolved), sha256_file(resolved), stat.st_size, stat.st_mtime_ns)


@dataclass(frozen=True)
class ToolReceipt:
    """Prove which producer revision/configuration emitted each artifact."""

    tool: str
    configuration: str
    source_root: str
    build_dir: str
    output_dir: str
    created_at: str
    artifacts: tuple[ArtifactEvidence, ...]

    def to_dict(self) -> dict[str, object]:
        """Return a stable JSON-compatible representation."""

        return {
            "schema": 1,
            "tool": self.tool,
            "configuration": self.configuration,
            "source_root": self.source_root,
            "build_dir": self.build_dir,
            "output_dir": self.output_dir,
            "created_at": self.created_at,
            "artifacts": [asdict(artifact) for artifact in self.artifacts],
        }

    @staticmethod
    def from_dict(value: dict[str, object]) -> "ToolReceipt":
        """Load a receipt while rejecting malformed artifact collections."""

        raw_artifacts = value.get("artifacts")
        if not isinstance(raw_artifacts, list):
            raise ValueError("tool receipt has no artifact list")
        artifacts = tuple(ArtifactEvidence(**item) for item in raw_artifacts if isinstance(item, dict))
        if len(artifacts) != len(raw_artifacts):
            raise ValueError("tool receipt contains an invalid artifact")
        return ToolReceipt(
            str(value["tool"]),
            str(value["configuration"]),
            str(value["source_root"]),
            str(value["build_dir"]),
            str(value["output_dir"]),
            str(value["created_at"]),
            artifacts,
        )


def create_tool_build_plan(
    options: ToolBuildOptions,
    run_id: str,
    history_dir: Path,
    work_dir: Path,
) -> ToolBuildPlan:
    """Create configure/build commands without executing the external producer."""

    selected = options.validated()
    definition = DEFINITIONS[selected.name]
    history = _a_path(history_dir, "tool history directory")
    work = _a_path(work_dir, "tool work directory")
    products = tuple(selected.output_dir / name for name in definition.required_products)
    optional = tuple(selected.output_dir / name for name in definition.optional_products)
    configure = _configure_command(selected)
    build = (
        "cmake",
        "--build",
        str(selected.build_dir),
        "--parallel",
        str(selected.jobs),
        "--config",
        selected.configuration,
        "--target",
        *definition.targets,
    )
    steps = (
        RunStep(f"{selected.name}-configure", f"Configure {selected.name.upper()}", configure, selected.source_root),
        RunStep(
            f"{selected.name}-build",
            f"Build {selected.name.upper()}",
            build,
            selected.source_root,
            freezes_compiler=True,
        ),
    )
    return ToolBuildPlan(run_id, selected, history, work, products[0], products, optional, steps)


def _configure_command(options: ToolBuildOptions) -> tuple[str, ...]:
    """Translate each producer's current CMake contract without hardcoded tool paths."""

    command = [
        "cmake",
        "-S",
        str(options.source_root),
        "-B",
        str(options.build_dir),
        "-G",
        "Ninja",
        f"-DCMAKE_BUILD_TYPE={options.configuration}",
    ]
    output = options.output_dir.as_posix()
    if options.name == "nslink":
        command.extend(
            (
                "-DCMAKE_CXX_COMPILER=clang-cl",
                "-DCMAKE_LINKER=lld-link",
                "-DNSLINK_VS2026_RUNTIME_ROOT=A:/vc2026-runtime",
                f"-DNSLINK_PRODUCT_OUTPUT_DIR={output}",
            )
        )
    elif options.name == "nsasm":
        command.append(f"-DNSASM_PRODUCT_OUTPUT_DIR={output}")
    else:
        command.extend(("-DNSDBG_BUILD_TESTS=OFF", f"-DNSDBG_PRODUCT_OUTPUT_DIR={output}"))
    return tuple(command)


def create_tool_receipt(plan: ToolBuildPlan, status: str) -> ToolReceipt:
    """Close a successful producer only when every required product exists."""

    if status != "passed":
        raise ValueError(f"cannot issue a receipt for {plan.options.name}: run status is {status}")
    artifacts = [ArtifactEvidence.capture(path) for path in plan.products]
    artifacts.extend(ArtifactEvidence.capture(path) for path in plan.optional_products if path.is_file())
    return ToolReceipt(
        plan.options.name,
        plan.options.configuration,
        str(plan.options.source_root),
        str(plan.options.build_dir),
        str(plan.options.output_dir),
        datetime.now(timezone.utc).isoformat(),
        tuple(artifacts),
    )


def validate_tool_receipt(receipt: ToolReceipt) -> tuple[Path, ...]:
    """Reject stale, replaced, missing, or incomplete tool products."""

    definition = DEFINITIONS.get(receipt.tool)
    if definition is None:
        raise ValueError(f"unsupported tool receipt: {receipt.tool}")
    observed: list[Path] = []
    for artifact in receipt.artifacts:
        path = Path(artifact.path).resolve()
        if not path.is_file():
            raise ValueError(f"tool receipt product is missing: {path}")
        stat = path.stat()
        if stat.st_size != artifact.size or stat.st_mtime_ns != artifact.modified_ns:
            raise ValueError(f"tool receipt product changed: {path}")
        if sha256_file(path) != artifact.sha256:
            raise ValueError(f"tool receipt product hash changed: {path}")
        observed.append(path)
    names = {path.name.casefold() for path in observed}
    missing = [name for name in definition.required_products if name.casefold() not in names]
    if missing:
        raise ValueError(f"tool receipt is incomplete for {receipt.tool}: {', '.join(missing)}")
    return tuple(observed)


def stage_tool_receipts(
    receipts: tuple[ToolReceipt, ...],
    compiler_bin: Path,
    manifest_path: Path,
) -> dict[str, object]:
    """Copy exact verified products beside NSC and emit provenance evidence."""

    destination = _a_path(compiler_bin, "compiler bin staging directory")
    manifest = _a_path(manifest_path, "tool staging manifest")
    destination.mkdir(parents=True, exist_ok=True)
    manifest.parent.mkdir(parents=True, exist_ok=True)
    entries: list[dict[str, object]] = []
    seen_tools: set[str] = set()
    for receipt in receipts:
        if receipt.tool in seen_tools:
            raise ValueError(f"duplicate tool receipt: {receipt.tool}")
        seen_tools.add(receipt.tool)
        for source in validate_tool_receipt(receipt):
            target = destination / source.name
            if source != target:
                shutil.copy2(source, target)
            entries.append(
                {
                    "tool": receipt.tool,
                    "configuration": receipt.configuration,
                    "source": str(source),
                    "destination": str(target),
                    "sha256": sha256_file(target),
                }
            )
    value: dict[str, object] = {
        "schema": 1,
        "created_at": datetime.now(timezone.utc).isoformat(),
        "compiler_bin": str(destination),
        "artifacts": entries,
    }
    temporary = manifest.with_suffix(manifest.suffix + ".tmp")
    temporary.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8", newline="\n")
    os.replace(temporary, manifest)
    return value
