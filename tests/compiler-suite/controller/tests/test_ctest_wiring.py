from __future__ import annotations

import ast
import re
import unittest
from pathlib import Path


SUITE_ROOT = Path(__file__).resolve().parents[2]
INTEGRATION_ROOT = SUITE_ROOT / "controller" / "integration"


def cmake_calls(text: str, command: str) -> tuple[str, ...]:
    calls: list[str] = []
    pattern = re.compile(rf"\b{re.escape(command)}\s*\(")
    for match in pattern.finditer(text):
        start = match.end() - 1
        depth = 0
        quoted = False
        escaped = False
        for cursor in range(start, len(text)):
            character = text[cursor]
            if escaped:
                escaped = False
                continue
            if character == "\\" and quoted:
                escaped = True
                continue
            if character == '"':
                quoted = not quoted
                continue
            if quoted:
                continue
            if character == "(":
                depth += 1
            elif character == ")":
                depth -= 1
                if depth == 0:
                    calls.append(text[start + 1 : cursor])
                    break
        else:
            raise AssertionError(f"unterminated {command} call")
    return tuple(calls)


def parser_options(path: Path) -> tuple[frozenset[str], frozenset[str]]:
    tree = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
    accepted: set[str] = set()
    required: set[str] = set()
    for node in ast.walk(tree):
        if not isinstance(node, ast.Call):
            continue
        function = node.func
        if not isinstance(function, ast.Attribute) or function.attr != "add_argument":
            continue
        options = {
            argument.value
            for argument in node.args
            if isinstance(argument, ast.Constant)
            and isinstance(argument.value, str)
            and argument.value.startswith("--")
        }
        accepted.update(options)
        is_required = any(
            keyword.arg == "required"
            and isinstance(keyword.value, ast.Constant)
            and keyword.value.value is True
            for keyword in node.keywords
        )
        if is_required:
            required.update(options)
    if not accepted:
        raise AssertionError(f"controller exposes no argparse options: {path}")
    return frozenset(accepted), frozenset(required)


class CTestWiringTests(unittest.TestCase):
    def test_every_controller_command_matches_its_argparse_contract(self) -> None:
        cmake = (SUITE_ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        wired: set[str] = set()
        for block in cmake_calls(cmake, "add_test"):
            scripts = re.findall(
                r"controller/integration/(run_[A-Za-z0-9_]+\.py)", block
            )
            if not scripts:
                continue
            self.assertEqual(len(scripts), 1, block)
            script_name = scripts[0]
            self.assertNotIn(script_name, wired)
            wired.add(script_name)
            accepted, required = parser_options(INTEGRATION_ROOT / script_name)
            provided = frozenset(
                re.findall(r"(?<![A-Za-z0-9_-])(--[a-z0-9-]+)(?==|\s|\")", block)
            )
            self.assertEqual(provided - accepted, frozenset(), script_name)
            self.assertEqual(required - provided, frozenset(), script_name)

        scripts = {path.name for path in INTEGRATION_ROOT.glob("run_*.py")}
        self.assertEqual(wired, scripts)


if __name__ == "__main__":
    unittest.main()
