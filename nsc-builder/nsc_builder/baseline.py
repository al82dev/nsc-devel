"""Baseline certification planning recovered from the original NSC Builder."""

from __future__ import annotations

import hashlib
import os
import shutil
from dataclasses import dataclass, replace
from pathlib import Path
from typing import Iterable

from .build import compiler_build_commands, compiler_configure_commands
from .configuration import BuilderConfig
from .paths import SOURCE_ROOT
from .toolchain import discover_compiler_build_tools
from .utils import sha256_file


BASELINE_PREFIX = Path("A:/native-sharp-3.0-baseline")
BASELINE_PROMOTION_PREFIX = Path("C:/native-sharp")
BASELINE_NSLINK_SOURCE = Path("A:/nslink-devel/bin/release/nslink.exe")
BASELINE_OPTIMIZATIONS = ("O0", "O1", "O2", "O3")
BASELINE_UNIT_CATEGORY = "Prueba unitaria"
BASELINE_EXTERNAL_CATEGORY = "Prueba externa"


@dataclass(frozen=True)
class BaselinePlan:
    """Immutable Gate 0 preparation, suite, and promotion contract."""

    staging_prefix: Path
    promotion_prefix: Path
    nslink_source: Path
    comparison_source: Path
    compiler_config: BuilderConfig
    optimizations: tuple[str, ...]
    preparation_commands: tuple[tuple[str, list[str], Path], ...]
    bootstrap_command: tuple[str, list[str], Path]


def create_baseline_plan(
    config: BuilderConfig,
    optimizations: Iterable[str] = BASELINE_OPTIMIZATIONS,
) -> BaselinePlan:
    """Create the complete fixed-root Gate 0 preparation plan."""
    selected = tuple(dict.fromkeys(optimizations))
    if not selected or not set(selected) <= set(BASELINE_OPTIMIZATIONS):
        raise ValueError("baseline optimizations must contain one or more of O0..O3")
    staging = BASELINE_PREFIX.resolve()
    destination = BASELINE_PROMOTION_PREFIX.resolve()
    nslink = _select_baseline_nslink_source(Path(config.nslink_executable), staging)
    comparison = _validated_comparison_source(
        Path(config.comparison_nsc or config.nsc_executable)
    )
    tools = discover_compiler_build_tools(str(config.llvm_root_path()))
    baseline_config = replace(
        config,
        install_prefix=str(staging),
        nsc_executable=str(staging / "bin" / "nsc.exe"),
        nslink_executable=str(staging / "bin" / "nslink.exe"),
    )
    configure = compiler_configure_commands(
        baseline_config, ("Release",), "Static", "O2"
    )
    build = compiler_build_commands(baseline_config, ("Release",))
    install = (
        "Instalar compilador en staging",
        [
            tools["cmake"],
            "--install",
            str(Path(baseline_config.compiler_build_root).resolve()),
            "--config",
            "Release",
            "--prefix",
            staging.as_posix(),
        ],
        SOURCE_ROOT,
    )
    bootstrap_output = staging / "lib" / "NativeSharpCore.lib"
    bootstrap = (
        "Construir bibliotecas bootstrap",
        [
            str(staging / "bin" / "nsc.exe"),
            "--emit-project-oop-lib",
            str(SOURCE_ROOT / "lib" / "NativeSharp.Core" / "NativeSharp.Core.nsproj"),
            "-O0",
            "-j",
            str(config.jobs),
            "-o",
            str(bootstrap_output),
        ],
        SOURCE_ROOT,
    )
    return BaselinePlan(
        staging,
        destination,
        nslink,
        comparison,
        baseline_config,
        selected,
        tuple([*configure, *build, install]),
        bootstrap,
    )


def reset_baseline_staging(path: Path = BASELINE_PREFIX) -> None:
    """Reset only the exact fixed baseline staging directory."""
    target = path.resolve()
    expected = BASELINE_PREFIX.resolve()
    if target != expected:
        raise ValueError(f"baseline staging must be exactly {expected}; got {target}")
    if target.exists():
        _remove_tree_iteratively(target)
    target.mkdir(parents=True)


def stage_baseline_nslink(source: Path, prefix: Path = BASELINE_PREFIX) -> Path:
    """Copy one verified A-drive NSLink into the fixed staging tree."""
    staging = prefix.resolve()
    if staging != BASELINE_PREFIX.resolve():
        raise ValueError(f"baseline staging must be exactly {BASELINE_PREFIX.resolve()}")
    verified = _validated_nslink_source(source, staging)
    destination = staging / "bin" / "nslink.exe"
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(verified, destination)
    if sha256_file(verified) != sha256_file(destination):
        raise OSError(f"staged NSLink hash mismatch: {destination}")
    return destination


def snapshot_baseline_comparison(source: Path, destination: Path) -> Path:
    """Preserve the pre-build compiler before Gate 0 mutates build and staging trees."""
    verified = _validated_comparison_source(source)
    target = destination.resolve()
    if target.drive.casefold() != "a:":
        raise ValueError(f"baseline comparison snapshot must stay on A:; got {target}")
    if target == verified:
        raise ValueError("baseline comparison snapshot must differ from its source")
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(verified, target)
    if sha256_file(verified) != sha256_file(target):
        raise OSError(f"baseline comparison snapshot hash mismatch: {target}")
    return target


def baseline_test_category(layer: str) -> str:
    """Project the canonical allocation layer onto the two GUI test surfaces."""
    return BASELINE_UNIT_CATEGORY if layer == "unit" else BASELINE_EXTERNAL_CATEGORY


def promote_certified_baseline(
    source: Path = BASELINE_PREFIX,
    destination: Path = BASELINE_PROMOTION_PREFIX,
) -> dict[str, str]:
    """Overlay the certified staging onto the fixed testing installation."""
    staging = source.resolve()
    target = destination.resolve()
    if staging != BASELINE_PREFIX.resolve():
        raise ValueError(f"unauthorized baseline source: {staging}")
    if target != BASELINE_PROMOTION_PREFIX.resolve():
        raise ValueError(f"unauthorized baseline destination: {target}")
    compiler = staging / "bin" / "nsc.exe"
    if not compiler.is_file():
        raise FileNotFoundError(f"baseline staging has no bin/nsc.exe: {compiler}")
    target.mkdir(parents=True, exist_ok=True)
    shutil.copytree(staging, target, dirs_exist_ok=True)
    installed = target / "bin" / "nsc.exe"
    return {
        "source": str(staging),
        "destination": str(target),
        "nsc_sha256": sha256_file(installed),
    }


def tree_fingerprint(paths: Iterable[Path]) -> str:
    """Hash stable source inputs used to prove an immobile certification tree."""
    digest = hashlib.sha256()
    for root in sorted((path.resolve() for path in paths), key=lambda item: str(item).casefold()):
        if not root.exists():
            continue
        files = [root] if root.is_file() else sorted(
            (path for path in root.rglob("*") if path.is_file()),
            key=lambda item: str(item).casefold(),
        )
        for path in files:
            relative = path.relative_to(SOURCE_ROOT) if path.is_relative_to(SOURCE_ROOT) else path
            digest.update(str(relative).replace("\\", "/").encode("utf-8"))
            digest.update(b"\0")
            digest.update(bytes.fromhex(sha256_file(path)))
    return digest.hexdigest()


def _validated_nslink_source(source: Path, staging: Path) -> Path:
    resolved = source.resolve()
    if resolved.drive.casefold() != "a:":
        raise ValueError(f"baseline NSLink must come from A:; got {resolved}")
    if resolved == staging or resolved.is_relative_to(staging):
        raise ValueError(f"baseline NSLink cannot come from reset staging: {resolved}")
    if not resolved.is_file():
        raise FileNotFoundError(f"baseline NSLink does not exist: {resolved}")
    return resolved


def _validated_comparison_source(source: Path) -> Path:
    resolved = source.resolve()
    if resolved.drive.casefold() != "a:":
        raise ValueError(f"baseline comparison NSC must come from A:; got {resolved}")
    if not resolved.is_file():
        raise FileNotFoundError(f"baseline comparison NSC does not exist: {resolved}")
    return resolved


def _select_baseline_nslink_source(configured: Path, staging: Path) -> Path:
    """Prefer a valid override, repairing the legacy reset-staging default."""
    resolved = configured.resolve()
    if resolved == staging or resolved.is_relative_to(staging):
        return _validated_nslink_source(BASELINE_NSLINK_SOURCE, staging)
    return _validated_nslink_source(configured, staging)


def _remove_tree_iteratively(root: Path) -> None:
    """Delete a validated tree bottom-up without recursive deletion helpers."""
    for current, directories, files in os.walk(root, topdown=False, followlinks=False):
        current_path = Path(current)
        for name in files:
            (current_path / name).unlink()
        for name in directories:
            child = current_path / name
            if child.is_symlink():
                child.unlink()
            else:
                child.rmdir()
    root.rmdir()


__all__ = (
    "BASELINE_EXTERNAL_CATEGORY",
    "BASELINE_OPTIMIZATIONS",
    "BASELINE_NSLINK_SOURCE",
    "BASELINE_PREFIX",
    "BASELINE_PROMOTION_PREFIX",
    "BASELINE_UNIT_CATEGORY",
    "BaselinePlan",
    "baseline_test_category",
    "create_baseline_plan",
    "promote_certified_baseline",
    "reset_baseline_staging",
    "snapshot_baseline_comparison",
    "stage_baseline_nslink",
    "tree_fingerprint",
)
