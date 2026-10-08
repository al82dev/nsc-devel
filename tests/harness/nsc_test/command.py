"""Construct NSC process requests from validated manifests."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from .model import CaseKind, TestCase
from .process import ProcessRequest
from .workspace import CaseWorkspace


LINK_KINDS = {CaseKind.LINK_RUN, CaseKind.EMIT_LIBRARY}


@dataclass(frozen=True)
class NscCommandBuilder:
    nsc: Path
    nslink: Path | None = None

    def __post_init__(self) -> None:
        resolved_nsc = self.nsc.resolve()
        if not resolved_nsc.is_file():
            raise ValueError(f"NSC does not exist: {resolved_nsc}")
        object.__setattr__(self, "nsc", resolved_nsc)
        if self.nslink is not None:
            resolved_nslink = self.nslink.resolve()
            if not resolved_nslink.is_file():
                raise ValueError(f"NSLink does not exist: {resolved_nslink}")
            object.__setattr__(self, "nslink", resolved_nslink)

    def build(self, case: TestCase, workspace: CaseWorkspace) -> ProcessRequest:
        if case.kind in LINK_KINDS and self.nslink is None:
            raise ValueError(f"case requires an explicit NSLink path: {case.identifier}")
        command = [str(self.nsc)]
        used_sources: set[int] = set()
        used_project = False
        for argument in case.arguments:
            replacement, source_index, project_used = self._replace_argument(
                argument, case, workspace
            )
            command.append(replacement)
            if source_index is not None:
                used_sources.add(source_index)
            used_project = used_project or project_used

        if case.kind is not CaseKind.SPECIALIZED:
            if "-O0" not in command:
                command.append("-O0")
            if case.project is not None and not used_project:
                command.append(str(workspace.mapped(case.project)))
            for index, source in enumerate(case.sources):
                if index not in used_sources:
                    command.append(str(workspace.mapped(source)))

        environment: dict[str, str] = {}
        environment["TEMP"] = str(workspace.root)
        environment["TMP"] = str(workspace.root)
        if self.nslink is not None:
            environment["NSC_LINKER"] = str(self.nslink)
        return ProcessRequest(
            command=tuple(command),
            working_directory=workspace.root,
            timeout_seconds=case.timeout_seconds,
            stdin=case.stdin,
            environment=environment,
        )

    @staticmethod
    def _replace_argument(
        argument: str,
        case: TestCase,
        workspace: CaseWorkspace,
    ) -> tuple[str, int | None, bool]:
        if argument == "{workspace}":
            return str(workspace.root), None, False
        workspace_prefix = "{workspace}/"
        if argument.startswith(workspace_prefix):
            relative = Path(argument[len(workspace_prefix) :])
            candidate = (workspace.root / relative).resolve()
            if not candidate.is_relative_to(workspace.root.resolve()):
                raise ValueError(f"workspace output escapes case sandbox: {argument}")
            return str(candidate), None, False
        if argument == "{project}":
            if case.project is None:
                raise ValueError(f"case has no project placeholder target: {case.identifier}")
            return str(workspace.mapped(case.project)), None, True
        if argument.startswith("{source:") and argument.endswith("}"):
            index_text = argument[len("{source:") : -1]
            if not index_text.isdigit():
                raise ValueError(f"invalid source placeholder: {argument}")
            index = int(index_text)
            if index >= len(case.sources):
                raise ValueError(f"source placeholder is out of range: {argument}")
            return str(workspace.mapped(case.sources[index])), index, False
        if "{" in argument or "}" in argument:
            raise ValueError(f"unknown command placeholder: {argument}")
        return argument, None, False
