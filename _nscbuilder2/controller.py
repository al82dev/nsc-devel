"""Stateful Builder 2 composition without GUI dependencies."""

from __future__ import annotations

from pathlib import Path

from .compiler import (
    CompilerBuildOptions,
    CompilerReceipt,
    create_compiler_build_plan,
    create_compiler_receipt,
    create_test_run_plan,
    validate_compiler_receipt,
)
from .deploy import create_deploy_plan
from .model import RunSelection
from .storage import BuilderStorage
from .toolchain import (
    ToolBuildOptions,
    ToolReceipt,
    create_tool_build_plan,
    create_tool_receipt,
    stage_tool_receipts,
    validate_tool_receipt,
)


class BuilderController:
    """Own receipt lifecycle and create plans for one Builder 2 session."""

    def __init__(self, storage: BuilderStorage) -> None:
        """Use explicit storage so tests and GUI never depend on global state."""

        self.storage = storage

    def compiler_plan(self, options: CompilerBuildOptions):
        """Allocate evidence directories and plan an isolated NSC build."""

        run_id, history, work = self.storage.new_run()
        return create_compiler_build_plan(options, run_id, history, work)

    def accept_compiler(self, plan, status: str) -> CompilerReceipt:
        """Persist the only active compiler receipt after a successful build."""

        receipt = create_compiler_receipt(plan, status)
        self.storage.save_compiler_receipt(receipt.to_dict())
        return receipt

    def compiler_receipt(self) -> CompilerReceipt | None:
        """Return a valid current receipt or None when tests must stay disabled."""

        value = self.storage.load_compiler_receipt()
        if value is None:
            return None
        receipt = CompilerReceipt.from_dict(value)
        validate_compiler_receipt(receipt)
        return receipt

    def tool_plan(self, options: ToolBuildOptions):
        """Allocate evidence directories and plan one external tool producer."""

        run_id, history, work = self.storage.new_run()
        return create_tool_build_plan(options, run_id, history, work)

    def accept_tool(self, plan, status: str) -> ToolReceipt:
        """Persist a tool receipt only after all declared products exist."""

        receipt = create_tool_receipt(plan, status)
        self.storage.save_tool_receipt(
            receipt.tool,
            receipt.configuration,
            receipt.to_dict(),
        )
        return receipt

    def tool_receipt(self, name: str, configuration: str) -> ToolReceipt | None:
        """Return one valid tool receipt or None when no receipt was issued."""

        value = self.storage.load_tool_receipt(name, configuration)
        if value is None:
            return None
        receipt = ToolReceipt.from_dict(value)
        validate_tool_receipt(receipt)
        return receipt

    def test_plan(self, selection: RunSelection):
        """Plan tests only when the active compiler receipt remains valid."""

        receipt = self.compiler_receipt()
        if receipt is None:
            raise ValueError("build NSC successfully before configuring tests")
        run_id, history, work = self.storage.new_run()
        return create_test_run_plan(receipt, selection, run_id, history, work)

    def stage_tools(self, names: tuple[str, ...], configuration: str) -> Path:
        """Stage selected tools beside the active A:-resident compiler."""

        compiler = self.compiler_receipt()
        if compiler is None:
            raise ValueError("build NSC successfully before staging tools")
        receipts = self._required_tools(names, configuration)
        compiler_bin = validate_compiler_receipt(compiler).parent
        manifest = self.storage.root / "staging" / "toolchain-manifest.json"
        stage_tool_receipts(receipts, compiler_bin, manifest)
        return manifest

    def deploy_plan(self, prefix: Path, names: tuple[str, ...], configuration: str):
        """Plan explicit installation with the selected verified tools."""

        compiler = self.compiler_receipt()
        if compiler is None:
            raise ValueError("build NSC successfully before deploy")
        receipts = self._required_tools(names, configuration)
        run_id, history, work = self.storage.new_run()
        return create_deploy_plan(compiler, receipts, prefix, run_id, history, work)

    def _required_tools(self, names: tuple[str, ...], configuration: str) -> tuple[ToolReceipt, ...]:
        """Resolve every selected receipt without silently omitting producers."""

        receipts: list[ToolReceipt] = []
        for name in names:
            receipt = self.tool_receipt(name, configuration)
            if receipt is None:
                raise ValueError(f"build {name.upper()} {configuration} before this operation")
            receipts.append(receipt)
        return tuple(receipts)
