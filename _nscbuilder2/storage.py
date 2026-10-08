"""Crash-surviving, Builder-2-only configuration and history storage."""

from __future__ import annotations

import json
import os
from datetime import datetime, timezone
from pathlib import Path
from uuid import uuid4

from .model import BuilderConfig


class BuilderStorage:
    def __init__(self, root: Path) -> None:
        self.root = root.resolve()
        if self.root.drive.upper() != "A:":
            raise ValueError(f"Builder 2 storage must stay on A: {self.root}")
        self.config_path = self.root / "config.json"
        self.compiler_receipt_path = self.root / "compiler-receipt.json"
        self.history_root = self.root / "history"
        self.work_root = self.root / "work"
        self.receipts_root = self.root / "receipts"

    def ensure(self) -> None:
        self.history_root.mkdir(parents=True, exist_ok=True)
        self.work_root.mkdir(parents=True, exist_ok=True)
        self.receipts_root.mkdir(parents=True, exist_ok=True)

    def new_run(self) -> tuple[str, Path, Path]:
        self.ensure()
        stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
        run_id = f"{stamp}-{uuid4().hex[:8]}"
        history = self.history_root / run_id
        work = self.work_root / run_id
        history.mkdir()
        work.mkdir()
        return run_id, history, work

    def load_config(self, fallback: BuilderConfig) -> BuilderConfig:
        if not self.config_path.is_file():
            return fallback
        value = json.loads(self.config_path.read_text(encoding="utf-8"))
        if not isinstance(value, dict):
            raise ValueError(f"invalid Builder 2 configuration: {self.config_path}")
        return BuilderConfig.from_dict(value)

    def save_config(self, config: BuilderConfig) -> None:
        self.ensure()
        temporary = self.config_path.with_suffix(".json.tmp")
        temporary.write_text(
            json.dumps(config.to_dict(), indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
            newline="\n",
        )
        os.replace(temporary, self.config_path)

    @staticmethod
    def write_run(history_dir: Path, value: dict[str, object]) -> None:
        target = history_dir / "run.json"
        temporary = history_dir / "run.json.tmp"
        temporary.write_text(
            json.dumps(value, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
            newline="\n",
        )
        os.replace(temporary, target)

    def load_history(self) -> list[dict[str, object]]:
        self.ensure()
        rows: list[dict[str, object]] = []
        for directory in sorted(self.history_root.iterdir(), reverse=True):
            record = directory / "run.json"
            if not directory.is_dir() or not record.is_file():
                continue
            try:
                value = json.loads(record.read_text(encoding="utf-8"))
            except (OSError, json.JSONDecodeError):
                continue
            if isinstance(value, dict):
                rows.append(value)
        return rows

    def save_tool_receipt(self, tool: str, configuration: str, value: dict[str, object]) -> Path:
        """Atomically persist one producer receipt under Builder 2 storage."""

        self.ensure()
        target = self.receipts_root / f"{tool.casefold()}-{configuration.casefold()}.json"
        temporary = target.with_suffix(".json.tmp")
        temporary.write_text(
            json.dumps(value, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
            newline="\n",
        )
        os.replace(temporary, target)
        return target

    def load_tool_receipt(self, tool: str, configuration: str) -> dict[str, object] | None:
        """Load one receipt without treating missing evidence as a valid build."""

        self.ensure()
        target = self.receipts_root / f"{tool.casefold()}-{configuration.casefold()}.json"
        if not target.is_file():
            return None
        value = json.loads(target.read_text(encoding="utf-8"))
        if not isinstance(value, dict):
            raise ValueError(f"invalid tool receipt: {target}")
        return value

    def save_compiler_receipt(self, value: dict[str, object]) -> Path:
        """Atomically replace the single active compiler build receipt."""

        self.ensure()
        temporary = self.compiler_receipt_path.with_suffix(".json.tmp")
        temporary.write_text(
            json.dumps(value, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
            newline="\n",
        )
        os.replace(temporary, self.compiler_receipt_path)
        return self.compiler_receipt_path

    def load_compiler_receipt(self) -> dict[str, object] | None:
        """Load the active compiler receipt or report that no build is ready."""

        if not self.compiler_receipt_path.is_file():
            return None
        value = json.loads(self.compiler_receipt_path.read_text(encoding="utf-8"))
        if not isinstance(value, dict):
            raise ValueError(f"invalid compiler receipt: {self.compiler_receipt_path}")
        return value
