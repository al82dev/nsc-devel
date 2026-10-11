"""Strict LLVM 23 toolchain selection shared by modular Builder operations."""

from __future__ import annotations

import re
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Callable


DEFAULT_LLVM_ROOT = Path("C:/clang-llvm")
REQUIRED_LLVM_MAJOR = 23
LLVM_PRODUCER_NAMES = ("clang_cl", "lld_link", "llvm_rc", "llvm_mt")


def llvm_root_from_configured_path(configured: str | Path) -> Path:
    """Normalize either an LLVM root or its canonical LLVM_DIR to one root."""
    path = Path(configured).expanduser().resolve()
    lowered = tuple(part.casefold() for part in path.parts[-3:])
    if lowered == ("lib", "cmake", "llvm"):
        return path.parents[2]
    return path


def selected_llvm_root(llvm_root: str, llvm_dir: str) -> Path:
    """Select one explicit LLVM authority, with C:/clang-llvm as the only default."""
    configured = llvm_root or llvm_dir or str(DEFAULT_LLVM_ROOT)
    return llvm_root_from_configured_path(configured)


def _llvm_config_version(config_path: Path) -> str:
    text = config_path.read_text(encoding="utf-8", errors="replace")
    match = re.search(
        r"set\(LLVM_PACKAGE_VERSION\s+[\"']?(\d+(?:\.\d+)+)", text
    )
    if match is None:
        raise ValueError(
            f"LLVMConfig.cmake does not publish LLVM_PACKAGE_VERSION: {config_path}"
        )
    return match.group(1)


def _llvm_producer_identity(path: Path) -> str:
    argument = "/?" if path.stem.casefold() in {"llvm-rc", "llvm-mt"} else "--version"
    completed = subprocess.run(
        [str(path), argument],
        text=True,
        encoding="utf-8",
        errors="replace",
        capture_output=True,
        check=False,
    )
    return (completed.stdout + "\n" + completed.stderr).strip()


def validate_llvm_toolchain(
    root: Path,
    producer_paths: dict[str, Path] | None = None,
    identity_reader: Callable[[Path], str] = _llvm_producer_identity,
) -> dict[str, str]:
    """Require one complete LLVM 23 toolchain rooted at one canonical directory."""
    root = root.expanduser().resolve()
    expected = {
        "clang_cl": root / "bin" / "clang-cl.exe",
        "lld_link": root / "bin" / "lld-link.exe",
        "llvm_rc": root / "bin" / "llvm-rc.exe",
        "llvm_mt": root / "bin" / "llvm-mt.exe",
    }
    producers = producer_paths or expected
    if set(producers) != set(LLVM_PRODUCER_NAMES):
        missing = sorted(set(LLVM_PRODUCER_NAMES) - set(producers))
        extra = sorted(set(producers) - set(LLVM_PRODUCER_NAMES))
        raise ValueError(
            "LLVM 23 producer set is incomplete"
            + (f"; missing: {', '.join(missing)}" if missing else "")
            + (f"; unexpected: {', '.join(extra)}" if extra else "")
        )
    resolved: dict[str, Path] = {}
    for name in LLVM_PRODUCER_NAMES:
        path = Path(producers[name]).expanduser().resolve()
        if path != expected[name].resolve():
            raise ValueError(
                f"LLVM 23 mixed-root producer rejected: {name}={path}; "
                f"expected {expected[name].resolve()}"
            )
        if not path.is_file():
            raise ValueError(f"LLVM 23 producer does not exist: {name}={path}")
        resolved[name] = path

    llvm_dir = root / "lib" / "cmake" / "llvm"
    llvm_config = llvm_dir / "LLVMConfig.cmake"
    if not llvm_config.is_file():
        raise ValueError(f"LLVM 23 configuration does not exist: {llvm_config}")
    config_version = _llvm_config_version(llvm_config)
    if int(config_version.split(".", 1)[0]) != REQUIRED_LLVM_MAJOR:
        raise ValueError(
            f"LLVM major {REQUIRED_LLVM_MAJOR} is required; "
            f"LLVMConfig.cmake reports {config_version} under {root}"
        )

    identities: dict[str, str] = {}
    identity_markers = {
        "clang_cl": "clang version",
        "lld_link": "LLD ",
        "llvm_rc": "LLVM Resource Converter",
        "llvm_mt": "Manifest Tool",
    }
    for name in LLVM_PRODUCER_NAMES:
        identity = identity_reader(resolved[name])
        if identity_markers[name].casefold() not in identity.casefold():
            raise ValueError(
                f"LLVM 23 producer identity is invalid: {name}={resolved[name]}"
            )
        version_match = re.search(
            r"\b(?:version\s+)?(\d+(?:\.\d+)+)\b", identity
        )
        version = version_match.group(1) if version_match else config_version
        if int(version.split(".", 1)[0]) != REQUIRED_LLVM_MAJOR:
            raise ValueError(
                f"LLVM major {REQUIRED_LLVM_MAJOR} is required; "
                f"{name} reports {version} at {resolved[name]}"
            )
        identities[f"{name}_version"] = version

    return {
        "llvm_root": str(root),
        "llvm_dir": str(llvm_dir.resolve()),
        "llvm_config": str(llvm_config.resolve()),
        "llvm_version": config_version,
        **{name: str(resolved[name]) for name in LLVM_PRODUCER_NAMES},
        **identities,
        "llvm_config_version": config_version,
    }


def discover_llvm_toolchain(configured: str | Path) -> dict[str, str]:
    """Resolve LLVM only below one configured root and validate its 23.x identity."""
    root = llvm_root_from_configured_path(configured or DEFAULT_LLVM_ROOT)
    return validate_llvm_toolchain(root)


def discover_build_tools(llvm_root: str) -> dict[str, str]:
    """Resolve build helpers and one strict, non-fallback LLVM 23 toolchain."""
    tools = {
        "cmake": shutil.which("cmake") or "",
        "ninja": shutil.which("ninja") or "",
        "python": str(Path(sys.executable).resolve()),
        **discover_llvm_toolchain(llvm_root),
    }
    missing = [name for name, value in tools.items() if not value]
    if missing:
        raise ValueError("required build tools were not found: " + ", ".join(missing))
    return tools


def discover_compiler_build_tools(llvm_root: str) -> dict[str, str]:
    """Resolve compiler-build inputs below the same validated LLVM 23 root."""
    llvm_tools = discover_llvm_toolchain(llvm_root)
    root = Path(llvm_tools["llvm_root"])
    tools = {
        "cmake": shutil.which("cmake") or "",
        "ninja": shutil.which("ninja") or "",
        **llvm_tools,
        "llvm_readobj": str((root / "bin" / "llvm-readobj.exe").resolve())
        if (root / "bin" / "llvm-readobj.exe").is_file()
        else "",
        "llvm_core": str((root / "lib" / "LLVMCore.lib").resolve())
        if (root / "lib" / "LLVMCore.lib").is_file()
        else "",
        "llvm_support": str((root / "lib" / "LLVMSupport.lib").resolve())
        if (root / "lib" / "LLVMSupport.lib").is_file()
        else "",
    }
    missing = [name for name, value in tools.items() if not value]
    if missing:
        raise ValueError("required NSC build tools were not found: " + ", ".join(missing))
    return tools


__all__ = (
    "DEFAULT_LLVM_ROOT",
    "LLVM_PRODUCER_NAMES",
    "REQUIRED_LLVM_MAJOR",
    "discover_build_tools",
    "discover_compiler_build_tools",
    "discover_llvm_toolchain",
    "llvm_root_from_configured_path",
    "selected_llvm_root",
    "validate_llvm_toolchain",
)
