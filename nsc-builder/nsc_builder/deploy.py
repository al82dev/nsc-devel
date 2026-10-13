"""Plan and validate an explicit NSC installation used for local testing."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from .build import compiler_build_dir
from .configuration import BuilderConfig
from .paths import SOURCE_ROOT
from .toolchain import discover_compiler_build_tools


DEPLOY_PREFIX = Path("C:/native-sharp")


@dataclass(frozen=True)
class DeployPlan:
    """Immutable two-step installation and verification contract."""

    build_mode: str
    build_dir: Path
    prefix: Path
    installed_compiler: Path
    commands: tuple[tuple[str, list[str], Path], ...]


def create_deploy_plan(config: BuilderConfig, build_mode: str) -> DeployPlan:
    """Recover the established CMake install flow without baseline coupling."""
    if build_mode not in {"Debug", "Release"}:
        raise ValueError(f"unsupported deploy build mode: {build_mode}")
    build_dir = compiler_build_dir(config, build_mode).resolve()
    install_script = build_dir / "cmake_install.cmake"
    if not install_script.is_file():
        raise ValueError(
            "the selected compiler build is not configured for installation: "
            f"{install_script}"
        )
    prefix = DEPLOY_PREFIX.resolve()
    if prefix == Path(prefix.anchor):
        raise ValueError(f"unsafe deploy prefix: {prefix}")
    if build_dir == prefix or prefix.is_relative_to(build_dir):
        raise ValueError(f"deploy prefix overlaps compiler build tree: {prefix}")
    tools = discover_compiler_build_tools(str(config.llvm_root_path()))
    compiler = prefix / "bin" / "nsc.exe"
    commands = (
        (
            f"Instalar NSC {build_mode}",
            [
                tools["cmake"],
                "--install",
                build_dir.as_posix(),
                "--config",
                build_mode,
                "--prefix",
                prefix.as_posix(),
            ],
            SOURCE_ROOT,
        ),
        (
            "Validar NSC instalado",
            [str(compiler), "--version", "--no-error-log"],
            SOURCE_ROOT,
        ),
    )
    return DeployPlan(build_mode, build_dir, prefix, compiler, commands)


__all__ = ("DEPLOY_PREFIX", "DeployPlan", "create_deploy_plan")
