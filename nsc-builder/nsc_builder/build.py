"""Extracted build implementation for modular NSC Builder 3."""

from __future__ import annotations

import argparse
import csv
import hashlib
import importlib.util
import json
import math
import os
import queue
import re
import shutil
import signal
import subprocess
import sys
import threading
import time
import uuid
from collections import Counter, defaultdict
from dataclasses import asdict, dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable, Iterable, Sequence

from .constants import *
from .paths import *
from .models import *
from .utils import *
from .configuration import *
from .toolchain import *


def effective_build_dir(config: BuilderConfig, mode: str) -> Path:
    """Keep Debug and Release test infrastructure in independent trees."""
    return Path(config.build_root).resolve() / mode.casefold()


def _has_reparse_attribute(path: Path) -> bool:
    """Detect Windows junctions and other reparse points without following them."""
    attributes = getattr(os.lstat(path), "st_file_attributes", 0)
    return bool(attributes & 0x400)


def _ensure_plain_path(path: Path, label: str) -> None:
    """Reject symlinks or junctions in an existing path before destructive work."""
    existing: list[Path] = []
    current = path
    while True:
        if current.exists() or current.is_symlink():
            existing.append(current)
        if current.parent == current:
            break
        current = current.parent
    for candidate in reversed(existing):
        if candidate.is_symlink() or _has_reparse_attribute(candidate):
            raise ValueError(f"{label} crosses a symbolic link or junction: {candidate}")


def _paths_overlap(left: Path, right: Path) -> bool:
    """Return whether either absolute path contains the other."""
    return left == right or left in right.parents or right in left.parents


def test_tree_cleanup_targets(config: BuilderConfig) -> tuple[Path, ...]:
    """Return the only test-build roots that Builder 3 is allowed to remove."""
    raw_root = Path(os.path.abspath(config.build_root))
    if not is_a_drive(raw_root):
        raise ValueError(f"test build root must be on A:, got {raw_root}")
    if raw_root.parent == raw_root:
        raise ValueError("test build root cannot be a drive root")
    _ensure_plain_path(raw_root, "test build root")
    root = raw_root.resolve()
    targets = tuple(root / mode.casefold() for mode in ("Debug", "Release"))

    protected = (
        SUITE_ROOT.resolve(),
        STORAGE_ROOT.resolve(),
        compiler_build_dir(config, "Debug").resolve(),
        compiler_build_dir(config, "Release").resolve(),
        Path(config.install_prefix).resolve(),
        Path(config.nsc_executable).resolve().parent,
        Path(config.nslink_executable).resolve().parent,
    )
    for target in targets:
        if target.parent != root:
            raise ValueError(f"test cleanup target escaped its configured root: {target}")
        if target == SOURCE_ROOT or target in SOURCE_ROOT.parents:
            raise ValueError(f"test cleanup target contains the source tree: {target}")
        if any(_paths_overlap(target, reserved) for reserved in protected):
            raise ValueError(f"test cleanup target overlaps a protected path: {target}")
        _ensure_plain_path(target, "test cleanup target")
    return targets


def clean_test_build_trees(config: BuilderConfig) -> TestTreeCleanupResult:
    """Delete selected generated trees with an explicit iterative post-order walk."""
    targets = test_tree_cleanup_targets(config)
    removed_files = 0
    removed_directories = 0
    missing: list[Path] = []
    failures: list[str] = []

    for root in targets:
        if not root.exists():
            missing.append(root)
            continue
        stack: list[tuple[Path, bool]] = [(root, False)]
        while stack:
            path, visited = stack.pop()
            try:
                if visited:
                    path.rmdir()
                    removed_directories += 1
                    continue
                if path.is_symlink() or _has_reparse_attribute(path):
                    attributes = getattr(os.lstat(path), "st_file_attributes", 0)
                    if attributes & 0x10:
                        path.rmdir()
                        removed_directories += 1
                    else:
                        path.unlink()
                        removed_files += 1
                    continue
                if not path.is_dir():
                    path.unlink()
                    removed_files += 1
                    continue
                stack.append((path, True))
                with os.scandir(path) as entries:
                    children = [Path(entry.path) for entry in entries]
                stack.extend((child, False) for child in children)
            except OSError as error:
                failures.append(f"{path}: {error}")

    return TestTreeCleanupResult(
        targets=tuple(targets),
        missing_targets=tuple(missing),
        removed_files=removed_files,
        removed_directories=removed_directories,
        failures=tuple(failures),
    )


def compiler_build_dir(config: BuilderConfig, mode: str) -> Path:
    """Keep Debug and Release compiler products in independent build trees."""
    release = Path(config.compiler_build_root).resolve()
    if mode == "Release":
        return release
    return release.parent / f"{release.name}-debug"


def compiler_output(config: BuilderConfig, mode: str) -> Path:
    """Return the NSC artifact produced by one compiler configuration."""
    return compiler_build_dir(config, mode) / "bin" / "nsc.exe"


def compiler_runtime_library(config: BuilderConfig) -> Path:
    """Return the runtime produced with the selected compiler identity."""
    if config.build_id_header:
        build_id_header = Path(config.build_id_header).resolve()
        if len(build_id_header.parents) < 3:
            raise ValueError(
                "El encabezado BuildId no permite identificar su directorio generated"
            )
        generated_root = build_id_header.parents[2]
    else:
        selected_nsc = Path(config.nsc_executable).resolve()
        generated_root = selected_nsc.parent.parent / "generated"
    return generated_root / "NativeSharp.Runtime.Win32.lib"


def component_test_executable(config: BuilderConfig, mode: str, target: str) -> Path:
    """Locate one named component-test executable in supported CMake layouts."""
    root = effective_build_dir(config, mode)
    candidates = (
        root / f"{target}.exe",
        root / mode / f"{target}.exe",
        root / "bin" / f"{target}.exe",
        root / "bin" / mode / f"{target}.exe",
    )
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    return candidates[0]


def host_executable(config: BuilderConfig, mode: str) -> Path:
    """Locate the ordinary consolidated host executable."""
    return component_test_executable(config, mode, "nsc_component_tests")


def destructive_host_executable(config: BuilderConfig, mode: str) -> Path:
    """Locate the isolated five-case destructive Host.AST executable."""
    return component_test_executable(config, mode, "nsc_component_tests_destructive")


def configure_commands(config: BuilderConfig, modes: Sequence[str]) -> list[tuple[str, list[str], Path]]:
    """Create explicit standalone CMake commands without building NSC."""
    tools = discover_build_tools(str(config.llvm_root_path()))
    runtime_library = compiler_runtime_library(config).resolve().as_posix()
    commands: list[tuple[str, list[str], Path]] = []
    for mode in modes:
        build = effective_build_dir(config, mode)
        command = [
            tools["cmake"],
            "-S",
            SUITE_ROOT.as_posix(),
            "-B",
            build.as_posix(),
            "-G",
            "Ninja",
            f"-DCMAKE_MAKE_PROGRAM={Path(tools['ninja']).as_posix()}",
            f"-DCMAKE_BUILD_TYPE={mode}",
            f"-DCMAKE_CXX_COMPILER={Path(tools['clang_cl']).as_posix()}",
            f"-DCMAKE_LINKER={Path(tools['lld_link']).as_posix()}",
            f"-DCMAKE_MT={Path(tools['llvm_mt']).as_posix()}",
            f"-DCMAKE_RC_COMPILER={Path(tools['llvm_rc']).as_posix()}",
            f"-DPython3_EXECUTABLE={Path(tools['python']).as_posix()}",
            # CMAKE_LINKER already selects lld-link. Clear a stale cache value
            # because -fuse-ld=lld is a compiler-driver option, not an lld-link option.
            "-DCMAKE_EXE_LINKER_FLAGS=",
            f"-DNSC_COMPILER_SUITE_NSC_EXECUTABLE="
            f"{Path(config.nsc_executable).resolve().as_posix()}",
            f"-DNSC_NSLINK_EXECUTABLE={Path(config.nslink_executable).resolve().as_posix()}",
            f"-DNSC_COMPILER_SUITE_DEFAULT_WINDOWS_RUNTIME_LIBRARY="
            f"{runtime_library}",
        ]
        command.append(f"-DLLVM_DIR={Path(tools['llvm_dir']).as_posix()}")
        if config.archive_executable:
            command.append(
                f"-DNSC_COMPILER_SUITE_ARCHIVE_EXECUTABLE="
                f"{Path(config.archive_executable).resolve().as_posix()}"
            )
        else:
            command.append("-UNSC_COMPILER_SUITE_ARCHIVE_EXECUTABLE")
        if config.comparison_nsc:
            command.append(
                f"-DNSC_COMPILER_SUITE_COMPARISON_NSC_EXECUTABLE="
                f"{Path(config.comparison_nsc).resolve().as_posix()}"
            )
        else:
            command.append("-UNSC_COMPILER_SUITE_COMPARISON_NSC_EXECUTABLE")
        if config.build_id_header:
            command.append(
                f"-DNSC_COMPILER_SUITE_BUILD_ID_HEADER="
                f"{Path(config.build_id_header).resolve().as_posix()}"
            )
        else:
            command.append("-UNSC_COMPILER_SUITE_BUILD_ID_HEADER")
        commands.append((f"Configurar {mode}", command, SOURCE_ROOT))
    return commands


def build_commands(config: BuilderConfig, modes: Sequence[str]) -> list[tuple[str, list[str], Path]]:
    """Build the ordinary and isolated host-test targets in selected configurations."""
    tools = discover_build_tools(str(config.llvm_root_path()))
    commands: list[tuple[str, list[str], Path]] = []
    for mode in modes:
        build = effective_build_dir(config, mode)
        command = [
            tools["cmake"],
            "--build",
            build.as_posix(),
            "--config",
            mode,
            "--target",
            "nsc_component_tests",
            "nsc_component_tests_destructive",
            "nsc_component_tests_source_extended",
            "--parallel",
            str(config.jobs),
        ]
        commands.append((f"Construir infraestructura {mode}", command, SOURCE_ROOT))
    return commands


def compiler_configure_commands(
    config: BuilderConfig,
    modes: Sequence[str],
    linkage: str,
    host_optimization: str,
) -> list[tuple[str, list[str], Path]]:
    """Configure NSC itself with the canonical Clang/LLVM/LLD Windows toolchain."""
    tools = discover_compiler_build_tools(str(config.llvm_root_path()))
    commands: list[tuple[str, list[str], Path]] = []
    for mode in modes:
        build = compiler_build_dir(config, mode)
        command = [
            tools["cmake"],
            "-S",
            SOURCE_ROOT.as_posix(),
            "-B",
            build.as_posix(),
            "-G",
            "Ninja",
            f"-DCMAKE_BUILD_TYPE={mode}",
            f"-DCMAKE_INSTALL_PREFIX={Path(config.install_prefix).resolve().as_posix()}",
            f"-DCMAKE_C_COMPILER={Path(tools['clang_cl']).as_posix()}",
            f"-DCMAKE_CXX_COMPILER={Path(tools['clang_cl']).as_posix()}",
            f"-DCMAKE_LINKER={Path(tools['lld_link']).as_posix()}",
            f"-DCMAKE_MT={Path(tools['llvm_mt']).as_posix()}",
            f"-DCMAKE_RC_COMPILER={Path(tools['llvm_rc']).as_posix()}",
            "-DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld",
            "-DCMAKE_DISABLE_FIND_PACKAGE_LLVM=FALSE",
            "-UNSC_LLVM_MONOLITHIC_LIBRARY",
            f"-DLLVM_DIR={Path(tools['llvm_dir']).as_posix()}",
            "-DNSC_BUILD_TESTS=OFF",
            "-DNSC_WARNINGS_AS_ERRORS=ON",
            f"-DNSC_HOST_LINKAGE={linkage.upper()}",
            f"-DNSC_HOST_OPTIMIZATION={host_optimization}",
            f"-DNSC_NSLINK_EXECUTABLE={Path(config.nslink_executable).resolve().as_posix()}",
            "-DNSC_BUILD_ID_OVERRIDE=",
        ]
        commands.append((f"Configurar NSC {mode}", command, SOURCE_ROOT))
    return commands


def compiler_build_commands(
    config: BuilderConfig, modes: Sequence[str]
) -> list[tuple[str, list[str], Path]]:
    """Build only the NSC target from previously configured compiler trees."""
    tools = discover_compiler_build_tools(str(config.llvm_root_path()))
    commands: list[tuple[str, list[str], Path]] = []
    for mode in modes:
        command = [
            tools["cmake"],
            "--build",
            compiler_build_dir(config, mode).as_posix(),
            "--target",
            "nsc",
            "--parallel",
            str(config.jobs),
            "--",
            "-k",
            "0",
        ]
        commands.append((f"Construir NSC {mode}", command, SOURCE_ROOT))
    return commands
