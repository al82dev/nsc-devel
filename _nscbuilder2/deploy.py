"""Explicit compiler installation and verified tool deployment planning."""

from __future__ import annotations

import argparse
import json
import os
import shutil
from dataclasses import dataclass
from pathlib import Path

from .compiler import CompilerReceipt, validate_compiler_receipt
from .model import RunStep
from .toolchain import ToolReceipt, sha256_file, validate_tool_receipt


@dataclass(frozen=True)
class DeployPlan:
    """Represent an explicit install followed by exact tool copies."""

    run_id: str
    receipt: CompilerReceipt
    prefix: Path
    history_dir: Path
    work_dir: Path
    compiler: Path
    manifest: Path
    steps: tuple[RunStep, ...]
    expected_compiler_sha256: str

    def to_dict(self) -> dict[str, object]:
        """Serialize deploy intent without executing it."""

        return {
            "kind": "deploy",
            "run_id": self.run_id,
            "prefix": str(self.prefix),
            "history_dir": str(self.history_dir),
            "work_dir": str(self.work_dir),
            "compiler": str(self.compiler),
            "manifest": str(self.manifest),
            "expected_compiler_sha256": self.expected_compiler_sha256,
            "steps": [step.to_dict() for step in self.steps],
        }


def create_deploy_plan(
    receipt: CompilerReceipt,
    tools: tuple[ToolReceipt, ...],
    prefix: Path,
    run_id: str,
    history_dir: Path,
    work_dir: Path,
) -> DeployPlan:
    """Validate every producer and create an explicit two-phase deploy plan."""

    compiler = validate_compiler_receipt(receipt)
    destination = prefix.resolve()
    if not destination.is_absolute() or destination == Path(destination.anchor):
        raise ValueError(f"unsafe deploy prefix: {destination}")
    history = _a_path(history_dir, "deploy history directory")
    work = _a_path(work_dir, "deploy work directory")
    manifest = work / "deploy-manifest.json"
    entries: list[dict[str, str]] = []
    seen: set[str] = set()
    for tool in tools:
        if tool.tool in seen:
            raise ValueError(f"duplicate deploy tool: {tool.tool}")
        seen.add(tool.tool)
        for source in validate_tool_receipt(tool):
            entries.append(
                {
                    "tool": tool.tool,
                    "source": str(source),
                    "destination": str(destination / "bin" / source.name),
                    "sha256": sha256_file(source),
                }
            )
    payload = {
        "schema": 1,
        "prefix": str(destination),
        "compiler": str(compiler),
        "compiler_sha256": receipt.artifact.sha256,
        "tools": entries,
    }
    manifest.parent.mkdir(parents=True, exist_ok=True)
    manifest.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8", newline="\n")
    install = RunStep(
        "deploy-compiler",
        f"Install NSC into {destination}",
        (
            "cmake",
            "--install",
            str(receipt.options.build_dir),
            "--config",
            receipt.options.configuration,
            "--prefix",
            str(destination),
        ),
        receipt.options.source_root,
    )
    copy_tools = RunStep(
        "deploy-tools",
        "Copy verified NativeSharp tools",
        ("python", "-B", "-m", "_nscbuilder2.deploy", "--apply", str(manifest)),
        receipt.options.source_root,
    )
    return DeployPlan(
        run_id,
        receipt,
        destination,
        history,
        work,
        compiler,
        manifest,
        (install, copy_tools),
        receipt.artifact.sha256,
    )


def apply_manifest(manifest_path: Path) -> None:
    """Apply exact file copies after revalidating each source hash."""

    manifest = _a_path(manifest_path, "deploy manifest")
    payload = json.loads(manifest.read_text(encoding="utf-8"))
    if not isinstance(payload, dict) or payload.get("schema") != 1:
        raise ValueError(f"unsupported deploy manifest: {manifest}")
    entries = payload.get("tools")
    if not isinstance(entries, list):
        raise ValueError(f"deploy manifest has no tool list: {manifest}")
    for entry in entries:
        if not isinstance(entry, dict):
            raise ValueError("deploy manifest contains an invalid tool entry")
        source = Path(str(entry["source"])).resolve()
        destination = Path(str(entry["destination"])).resolve()
        if not source.is_file() or sha256_file(source) != str(entry["sha256"]):
            raise ValueError(f"deploy source is stale: {source}")
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
        if sha256_file(destination) != str(entry["sha256"]):
            raise ValueError(f"deployed product hash mismatch: {destination}")


def _a_path(value: str | Path, label: str) -> Path:
    """Keep all Builder 2 evidence and manifests on A:."""

    path = Path(value).resolve()
    if path.drive.upper() != "A:":
        raise ValueError(f"{label} must stay on A: {path}")
    return path


def main(argv: list[str] | None = None) -> int:
    """Expose only the private manifest application operation."""

    parser = argparse.ArgumentParser(description="Apply a verified Builder 2 deploy manifest")
    parser.add_argument("--apply", required=True, type=Path)
    arguments = parser.parse_args(argv)
    apply_manifest(arguments.apply)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
