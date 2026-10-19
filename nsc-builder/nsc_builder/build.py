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


NSLINK_SOURCE_ROOT = Path("A:/nslink-devel")
NSASM_SOURCE_ROOT = Path("A:/nsasm-devel")
NSDBG_SOURCE_ROOT = Path("A:/nsc-debugger")


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


def _is_within(path: Path, root: Path) -> bool:
    """Return whether path is root or one of its descendants."""
    return path == root or root in path.parents


def _ensure_generated_compiler_tree(path: Path) -> None:
    """Reject a populated directory that is not recognizably a CMake build tree."""
    if not path.exists():
        return
    if not path.is_dir():
        raise ValueError(f"compiler cleanup target is not a directory: {path}")
    with os.scandir(path) as entries:
        if next(entries, None) is None:
            return
    cache = path / "CMakeCache.txt"
    ninja = path / "build.ninja"
    if not cache.is_file() or not ninja.is_file():
        raise ValueError(
            "compiler cleanup target is populated but has no complete CMake/Ninja "
            f"identity: {path}"
        )
    source_line = next(
        (
            line
            for line in cache.read_text(encoding="utf-8", errors="replace").splitlines()
            if line.startswith("CMAKE_HOME_DIRECTORY:INTERNAL=")
        ),
        "",
    )
    if not source_line:
        raise ValueError(f"compiler cleanup target has no CMake source identity: {path}")
    configured_source = Path(source_line.split("=", 1)[1]).resolve()
    if configured_source != SOURCE_ROOT.resolve():
        raise ValueError(
            "compiler cleanup target belongs to another source tree: "
            f"{path} -> {configured_source}"
        )


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
    return _clean_generated_build_trees(targets)


def _clean_generated_build_trees(
    targets: Sequence[Path],
) -> TestTreeCleanupResult:
    """Delete exact generated roots with an iterative post-order walk."""
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


def compiler_build_cleanup_targets(config: BuilderConfig) -> tuple[Path, ...]:
    """Return the exact Debug and Release compiler build trees safe to remove."""
    raw_release = Path(os.path.abspath(config.compiler_build_root))
    if not is_a_drive(raw_release):
        raise ValueError(f"compiler build root must be on A:, got {raw_release}")
    if raw_release.parent == raw_release:
        raise ValueError("compiler build root cannot be a drive root")
    _ensure_plain_path(raw_release, "compiler build root")
    targets = (
        compiler_build_dir(config, "Debug").resolve(),
        compiler_build_dir(config, "Release").resolve(),
    )
    if len(set(targets)) != 2:
        raise ValueError("Debug and Release compiler build trees must be distinct")
    test_root = Path(config.build_root).resolve()
    protected = (
        SUITE_ROOT.resolve(),
        STORAGE_ROOT.resolve(),
        HISTORY_ROOT.resolve(),
        CONFIG_PATH.resolve(),
        test_root,
        effective_build_dir(config, "Debug").resolve(),
        effective_build_dir(config, "Release").resolve(),
        Path(config.install_prefix).resolve(),
        Path(config.nslink_executable).resolve(),
        config.llvm_root_path().resolve(),
    )
    source = SOURCE_ROOT.resolve()
    for target in targets:
        if target == source or target in source.parents:
            raise ValueError(f"compiler cleanup target contains the source tree: {target}")
        if any(_paths_overlap(target, reserved) for reserved in protected):
            raise ValueError(f"compiler cleanup target overlaps a protected path: {target}")
        _ensure_plain_path(target, "compiler cleanup target")
        _ensure_generated_compiler_tree(target)
    return targets


def clean_compiler_build_trees(config: BuilderConfig) -> TestTreeCleanupResult:
    """Delete only the validated compiler build trees owned by the modular Builder."""
    return _clean_generated_build_trees(compiler_build_cleanup_targets(config))


def compiler_cleanup_invalidated_fields(
    config: BuilderConfig, targets: Sequence[Path]
) -> tuple[str, ...]:
    """Identify transient compiler artifact selections invalidated by cleanup."""
    invalidated: list[str] = []
    for field_name in ("nsc_executable", "comparison_nsc", "build_id_header"):
        value = getattr(config, field_name, "")
        if not value:
            continue
        path = Path(value).resolve()
        if any(_is_within(path, target.resolve()) for target in targets):
            invalidated.append(field_name)
    return tuple(invalidated)


def compiler_output(config: BuilderConfig, mode: str) -> Path:
    """Return the NSC artifact produced by one compiler configuration."""
    return compiler_build_dir(config, mode) / "bin" / "nsc.exe"


def compiler_sibling_output(config: BuilderConfig, mode: str, name: str) -> Path:
    """Return one Win32 sibling tool published beside the selected NSC build."""
    if name not in {"nslink", "nsasm", "nsdbg"}:
        raise ValueError(f"unsupported NativeSharp sibling tool: {name}")
    return compiler_build_dir(config, mode) / "bin" / f"{name}.exe"


def _sibling_build_dir(source_root: Path, mode: str) -> Path:
    """Keep Builder-owned sibling builds isolated by product and configuration."""
    return source_root / f"build-nscbuilder3-{mode.casefold()}"


def _validated_sibling_source(path: Path, label: str) -> Path:
    """Require the fixed A-drive Win32 source checkout and its CMake entry point."""
    source = require_directory_path(path, label, require_a_drive=True, must_exist=True)
    require_file_path(source / "CMakeLists.txt", f"{label} CMakeLists", require_a_drive=True)
    return source


def compiler_nslink_commands(
    config: BuilderConfig, modes: Sequence[str]
) -> list[tuple[str, list[str], Path]]:
    """Configure and build the Win32 NSLink consumed by each NSC configuration."""
    tools = discover_compiler_build_tools(str(config.llvm_root_path()))
    source = _validated_sibling_source(NSLINK_SOURCE_ROOT, "NSLink source root")
    llvm_config = Path(tools["llvm_root"]) / "bin" / "llvm-config.exe"
    if not llvm_config.is_file():
        raise ValueError(f"required LLVM 23 tool does not exist: {llvm_config}")
    commands: list[tuple[str, list[str], Path]] = []
    for mode in modes:
        build = _sibling_build_dir(source, mode)
        output = compiler_sibling_output(config, mode, "nslink").parent
        commands.append(
            (
                f"Configurar NSLink {mode}",
                [
                    tools["cmake"],
                    "-S",
                    source.as_posix(),
                    "-B",
                    build.as_posix(),
                    "-G",
                    "Ninja",
                    f"-DCMAKE_MAKE_PROGRAM={Path(tools['ninja']).as_posix()}",
                    f"-DCMAKE_BUILD_TYPE={mode}",
                    f"-DCMAKE_CXX_COMPILER={Path(tools['clang_cl']).as_posix()}",
                    f"-DCMAKE_LINKER={Path(tools['lld_link']).as_posix()}",
                    f"-DNSLINK_LLVM_CONFIG={llvm_config.as_posix()}",
                    f"-DNSLINK_PRODUCT_OUTPUT_DIR={output.as_posix()}",
                ],
                source,
            )
        )
        commands.append(
            (
                f"Construir NSLink {mode}",
                [
                    tools["cmake"],
                    "--build",
                    build.as_posix(),
                    "--target",
                    "nslink",
                    "--parallel",
                    str(config.jobs),
                ],
                source,
            )
        )
    return commands


def compiler_post_build_sibling_commands(
    config: BuilderConfig, modes: Sequence[str]
) -> list[tuple[str, list[str], Path]]:
    """Build NSAsm and then NSDBG beside each completed Win32 NSC build."""
    tools = discover_compiler_build_tools(str(config.llvm_root_path()))
    llvm_root = Path(tools["llvm_root"]).resolve()
    nsasm_source = _validated_sibling_source(NSASM_SOURCE_ROOT, "NSAsm source root")
    nsdbg_source = _validated_sibling_source(NSDBG_SOURCE_ROOT, "NSDBG source root")
    commands: list[tuple[str, list[str], Path]] = []
    for mode in modes:
        compiler_build = compiler_build_dir(config, mode)
        output = compiler_build / "bin"
        nsasm_build = _sibling_build_dir(nsasm_source, mode)
        nsdbg_build = _sibling_build_dir(nsdbg_source, mode)
        commands.extend(
            [
                (
                    f"Configurar NSAsm {mode}",
                    [
                        tools["cmake"],
                        "-S",
                        nsasm_source.as_posix(),
                        "-B",
                        nsasm_build.as_posix(),
                        "-G",
                        "Ninja",
                        f"-DCMAKE_MAKE_PROGRAM={Path(tools['ninja']).as_posix()}",
                        f"-DCMAKE_BUILD_TYPE={mode}",
                        f"-DCMAKE_INSTALL_PREFIX={compiler_build.as_posix()}",
                        f"-DNATIVESHARP_LLVM_ROOT={llvm_root.as_posix()}",
                        f"-DNSASM_PRODUCT_OUTPUT_DIR={output.as_posix()}",
                    ],
                    nsasm_source,
                ),
                (
                    f"Construir NSAsm {mode}",
                    [
                        tools["cmake"],
                        "--build",
                        nsasm_build.as_posix(),
                        "--target",
                        "nsasm",
                        "--parallel",
                        str(config.jobs),
                    ],
                    nsasm_source,
                ),
                (
                    f"Configurar NSDBG {mode}",
                    [
                        tools["cmake"],
                        "-S",
                        nsdbg_source.as_posix(),
                        "-B",
                        nsdbg_build.as_posix(),
                        "-G",
                        "Ninja",
                        f"-DCMAKE_MAKE_PROGRAM={Path(tools['ninja']).as_posix()}",
                        f"-DCMAKE_BUILD_TYPE={mode}",
                        f"-DCMAKE_INSTALL_PREFIX={compiler_build.as_posix()}",
                        f"-DNATIVESHARP_LLVM_ROOT={llvm_root.as_posix()}",
                        "-DNSDBG_BUILD_TESTS=OFF",
                        f"-DNSDBG_NATIVE_SHARP_ROOT={compiler_build.as_posix()}",
                        f"-DNSDBG_PRODUCT_OUTPUT_DIR={output.as_posix()}",
                        f"-DNSDBG_ZLIB_LIBRARY={(compiler_build / 'lib' / 'nsc-zlib.lib').as_posix()}",
                        f"-DNSDBG_ZSTD_LIBRARY={(compiler_build / 'lib' / 'nsc-zstd.lib').as_posix()}",
                        f"-DNSDBG_NSASM_EXECUTABLE={compiler_sibling_output(config, mode, 'nsasm').as_posix()}",
                        f"-DNSDBG_NSLINK_EXECUTABLE={compiler_sibling_output(config, mode, 'nslink').as_posix()}",
                        f"-DNSDBG_NSC_EXECUTABLE={compiler_output(config, mode).as_posix()}",
                    ],
                    nsdbg_source,
                ),
                (
                    f"Construir NSDBG {mode}",
                    [
                        tools["cmake"],
                        "--build",
                        nsdbg_build.as_posix(),
                        "--target",
                        "nsdbg",
                        "--parallel",
                        str(config.jobs),
                    ],
                    nsdbg_source,
                ),
            ]
        )
    return commands


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


def test_producer_for_case(case: CaseRecord) -> TestProducer | None:
    """Derive a host producer from its canonical executor, without a closed target list."""
    if not case.executor.startswith("nsc_component_tests"):
        return None
    target = case.executor
    return TestProducer(
        key=target,
        target=target,
        executable_name=f"{target}.exe",
    )


def plan_test_producers(
    config: BuilderConfig, variants: Sequence[RunVariant]
) -> tuple[ProducerPlan, ...]:
    """Group selected consumers by their canonical producer and build mode."""
    consumers: dict[tuple[str, str], list[str]] = defaultdict(list)
    producers: dict[tuple[str, str], TestProducer] = {}
    for variant in variants:
        producer = test_producer_for_case(variant.case)
        if producer is None:
            continue
        identity = (producer.key, variant.build_mode)
        producers[identity] = producer
        consumers[identity].append(variant.key)
    plans: list[ProducerPlan] = []
    for identity in sorted(producers, key=lambda item: (item[1], item[0])):
        producer = producers[identity]
        mode = identity[1]
        plans.append(
            ProducerPlan(
                producer=producer,
                build_mode=mode,
                executable=component_test_executable(config, mode, producer.target),
                consumer_keys=tuple(sorted(set(consumers[identity]))),
            )
        )
    return tuple(plans)


def test_build_tree_is_configured(config: BuilderConfig, mode: str) -> bool:
    """Return whether the selected test tree has a complete Ninja configuration."""
    root = effective_build_dir(config, mode)
    return (root / "CMakeCache.txt").is_file() and (root / "build.ninja").is_file()


def producer_preparation_commands(
    config: BuilderConfig, plans: Sequence[ProducerPlan]
) -> list[PreparationCommand]:
    """Create a deterministic configure/build barrier for every required producer."""
    if not plans:
        return []
    by_mode: dict[str, list[ProducerPlan]] = defaultdict(list)
    for plan in plans:
        by_mode[plan.build_mode].append(plan)
    commands: list[PreparationCommand] = []
    tools = discover_build_tools(str(config.llvm_root_path()))
    for mode in sorted(by_mode):
        mode_plans = sorted(by_mode[mode], key=lambda item: item.producer.target)
        producer_keys = tuple(plan.key for plan in mode_plans)
        if not test_build_tree_is_configured(config, mode):
            label, command, cwd = configure_commands(config, (mode,))[0]
            commands.append(
                PreparationCommand(
                    stage="configure-tests",
                    label=label,
                    command=tuple(command),
                    cwd=cwd,
                    producer_keys=producer_keys,
                )
            )
        build = effective_build_dir(config, mode)
        for plan in mode_plans:
            commands.append(
                PreparationCommand(
                    stage="build-producer",
                    label=f"Preparar pruebas {mode}: {plan.producer.target}",
                    command=(
                        tools["cmake"],
                        "--build",
                        build.as_posix(),
                        "--config",
                        mode,
                        "--target",
                        plan.producer.target,
                        "--parallel",
                        str(config.jobs),
                    ),
                    cwd=SOURCE_ROOT,
                    producer_keys=(plan.key,),
                )
            )
    return commands


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
    use_sibling_nslink: bool = False,
) -> list[tuple[str, list[str], Path]]:
    """Configure NSC itself with the canonical Clang/LLVM/LLD Windows toolchain."""
    tools = discover_compiler_build_tools(str(config.llvm_root_path()))
    commands: list[tuple[str, list[str], Path]] = []
    for mode in modes:
        build = compiler_build_dir(config, mode)
        selected_nslink = (
            compiler_sibling_output(config, mode, "nslink")
            if use_sibling_nslink
            else Path(config.nslink_executable).resolve()
        )
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
            f"-DNSC_NSLINK_EXECUTABLE={selected_nslink.as_posix()}",
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
