"""Extracted configuration implementation for modular NSC Builder 3."""

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
import signal
import subprocess
import sys
import threading
import time
import uuid
from collections import Counter, defaultdict
from dataclasses import asdict, dataclass, field, replace
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable, Iterable, Sequence

from .constants import *
from .paths import *
from .utils import *
from .toolchain import *


def parse_builder_config_payload(value: Any) -> dict[str, Any]:
    """Validate persisted Builder 3 settings before constructing the dataclass."""
    if not isinstance(value, dict):
        raise ValueError("Builder 3 configuration must be a JSON object")
    allowed = set(CONFIG_STRING_FIELDS) | {"jobs"}
    unknown = sorted(set(value) - allowed)
    if unknown:
        raise ValueError("Builder 3 configuration contains unknown fields: " + ", ".join(unknown))
    for field_name in CONFIG_STRING_FIELDS:
        if field_name in value and not isinstance(value[field_name], str):
            raise ValueError(f"Builder 3 configuration {field_name} must be a string")
    if "jobs" in value and type(value["jobs"]) is not int:
        raise ValueError("Builder 3 configuration jobs must be an integer")
    if "jobs" in value and not 1 <= value["jobs"] <= 256:
        raise ValueError("parallel jobs must be between 1 and 256")
    return dict(value)


def discover_archive_executable(configured: str) -> str:
    """Find 7-Zip only from an explicit A-drive path or canonical A roots."""
    candidates: tuple[str | Path, ...] = (
        configured,
        Path("A:/apps/7zip/7z.exe"),
        Path("A:/7z/7z.exe"),
    )
    for candidate in candidates:
        if not candidate:
            continue
        raw_path = Path(candidate)
        if not is_a_drive(raw_path):
            continue
        path = raw_path.resolve()
        if path.is_file():
            return str(path)
    return ""


@dataclass(frozen=True)
class BuilderConfig:
    """Persist user-selected tools and output roots without implicit deployment."""

    nsc_executable: str = "A:/native-sharp-3.0-baseline/bin/nsc.exe"
    nslink_executable: str = "A:/native-sharp-3.0-baseline/bin/nslink.exe"
    compiler_build_root: str = "A:/nsc-devel/build-clang"
    install_prefix: str = "A:/native-sharp-3.0-baseline"
    build_root: str = "A:/nsc-devel/build-compiler-suite-v3"
    compiler_llvm_root: str = ""
    llvm_dir: str = ""
    archive_executable: str = ""
    comparison_nsc: str = ""
    build_id_header: str = ""
    jobs: int = max(1, min(32, os.cpu_count() or 1))

    @classmethod
    def load(cls) -> "BuilderConfig":
        """Load saved overrides and repair missing tools with known local locations."""
        if CONFIG_PATH.is_file():
            value = json.loads(
                CONFIG_PATH.read_text(encoding="utf-8"),
                object_pairs_hook=unique_json_object,
            )
            base = cls(**parse_builder_config_payload(value))
        else:
            base = cls()
        return base.with_discovery()

    def with_discovery(self) -> "BuilderConfig":
        """Fill absent or stale tool paths while preserving valid user overrides."""
        compiler_build_root = self.compiler_build_root
        if Path(compiler_build_root).name.casefold() == "build-clang-v3":
            compiler_build_root = "A:/nsc-devel/build-clang"
        nsc = first_existing_file(
            [
                self.nsc_executable,
                "A:/native-sharp-3.0-baseline/bin/nsc.exe",
                "A:/nsc-devel/build-clang/bin/nsc.exe",
                "A:/nsc-devel/build-clang/nsc.exe",
            ]
        ) or self.nsc_executable
        nslink = first_existing_file(
            [
                Path(nsc).parent / "nslink.exe",
                self.nslink_executable,
                "A:/nslink-devel/bin/release/nslink.exe",
                "A:/native-sharp-3.0-baseline/bin/nslink.exe",
            ]
        ) or self.nslink_executable
        compiler_llvm_root_path = selected_llvm_root(
            self.compiler_llvm_root, self.llvm_dir
        )
        compiler_llvm_root = str(compiler_llvm_root_path)
        llvm_dir = str(compiler_llvm_root_path / "lib" / "cmake" / "llvm")
        archive = discover_archive_executable(self.archive_executable)
        discovered = BuilderConfig(
            nsc_executable=nsc,
            nslink_executable=nslink,
            compiler_build_root=compiler_build_root,
            install_prefix=self.install_prefix,
            build_root=self.build_root,
            compiler_llvm_root=compiler_llvm_root,
            llvm_dir=llvm_dir,
            archive_executable=archive,
            comparison_nsc=self.comparison_nsc,
            build_id_header=self.build_id_header,
            jobs=self.jobs,
        )
        return discovered.with_canonical_identity_inputs()

    def with_canonical_identity_inputs(self) -> "BuilderConfig":
        """Resolve identity inputs only from the selected compiler roots."""
        comparison_nsc = first_distinct_existing_file(
            [
                self.comparison_nsc,
                Path(self.install_prefix) / "bin" / "nsc.exe",
                Path(self.compiler_build_root) / "bin" / "nsc.exe",
            ],
            self.nsc_executable,
        )
        build_id_header = first_existing_file(
            [
                self.build_id_header,
                Path(self.compiler_build_root)
                / "generated"
                / "nativesharp"
                / "Basic"
                / "BuildId.h",
            ]
        )
        return replace(
            self,
            comparison_nsc=comparison_nsc,
            build_id_header=build_id_header,
        )

    def save(self) -> None:
        """Persist settings under the repository-owned Builder 3 directory."""
        payload_source = asdict(self)
        llvm_root = self.llvm_root_path()
        payload_source["compiler_llvm_root"] = str(llvm_root)
        payload_source["llvm_dir"] = str(llvm_root / "lib" / "cmake" / "llvm")
        payload = parse_builder_config_payload(payload_source)
        self.validated(require_tools=False)
        write_json_atomic(CONFIG_PATH, payload)

    def validated(self, require_tools: bool = False) -> "BuilderConfig":
        """Validate writable roots and, when requested, external tool receipts."""
        require_directory_path(
            Path(self.build_root), "test build root", require_a_drive=True
        )
        require_directory_path(
            Path(self.compiler_build_root), "compiler build root", require_a_drive=True
        )
        require_directory_path(
            Path(self.install_prefix), "compiler install prefix", require_a_drive=True
        )
        require_directory_path(
            STORAGE_ROOT, "Builder 3 storage root", require_a_drive=True
        )
        if not 1 <= self.jobs <= 256:
            raise ValueError("parallel jobs must be between 1 and 256")
        if require_tools:
            require_file_path(
                Path(self.nsc_executable), "NSC executable", require_a_drive=True
            )
            require_file_path(
                Path(self.nslink_executable), "NSLink executable", require_a_drive=True
            )
            if self.archive_executable:
                require_file_path(
                    Path(self.archive_executable),
                    "archive executable",
                    require_a_drive=True,
                )
            if self.comparison_nsc:
                require_file_path(Path(self.comparison_nsc), "comparison NSC executable")
            if self.build_id_header:
                require_file_path(Path(self.build_id_header), "build-id header")
            llvm_root = self.llvm_root_path()
            require_directory_path(llvm_root, "NSC LLVM 23 root", must_exist=True)
            require_directory_path(
                llvm_root / "lib" / "cmake" / "llvm",
                "derived LLVM_DIR",
                must_exist=True,
            )
            discover_llvm_toolchain(self.llvm_root_path())
        return self

    def llvm_root_path(self) -> Path:
        """Return the single LLVM root that owns tools and LLVMConfig.cmake."""
        return selected_llvm_root(self.compiler_llvm_root, self.llvm_dir)
