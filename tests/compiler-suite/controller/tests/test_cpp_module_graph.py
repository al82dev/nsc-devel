from __future__ import annotations

import re
import unittest
from collections import Counter
from pathlib import Path


SUITE_ROOT = Path(__file__).resolve().parents[2]
MODULE_ROOTS = (
    SUITE_ROOT / "host" / "modules",
    SUITE_ROOT / "selfhost" / "modules",
    SUITE_ROOT / "modules",
)
REGISTER_DEFINITION = re.compile(
    r"\bvoid\s+(register[A-Za-z0-9_]+Module)\s*\(\s*TestRegistry&\s+registry\s*\)"
)
REGISTER_DECLARATION = re.compile(
    r"\bvoid\s+(register[A-Za-z0-9_]+Module)\s*\(\s*TestRegistry&\s+registry\s*\)\s*;"
)
REGISTER_CALL = re.compile(
    r"\bnsc::suite::(register[A-Za-z0-9_]+Module)\s*\(\s*registry\s*\)\s*;"
)


class CppModuleGraphTests(unittest.TestCase):
    @staticmethod
    def executable_block(cmake: str) -> str:
        executable = re.search(
            r"add_executable\s*\(\s*nsc_component_tests\s+(.*?)"
            r"(?=\ntarget_include_directories\s*\()",
            cmake,
            flags=re.DOTALL,
        )
        if executable is None:
            raise AssertionError("nsc_component_tests add_executable block is missing")
        return executable.group(1)

    def test_module_sources_cmake_declarations_and_calls_are_bijective(self) -> None:
        module_paths = sorted(
            path
            for root in MODULE_ROOTS
            for path in root.glob("*Module.cpp")
        )
        definitions: dict[str, Path] = {}
        for path in module_paths:
            matches = REGISTER_DEFINITION.findall(path.read_text(encoding="utf-8"))
            self.assertEqual(matches, list(dict.fromkeys(matches)), path)
            self.assertEqual(len(matches), 1, path)
            name = matches[0]
            self.assertNotIn(name, definitions, name)
            definitions[name] = path

        cmake = (SUITE_ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        executable = self.executable_block(cmake)
        cmake_modules = {
            token.replace("\\", "/")
            for token in re.findall(
                r"((?:host/modules|selfhost/modules|modules)/"
                r"[A-Za-z0-9_-]+Module\.cpp)",
                executable,
            )
        }
        filesystem_modules = {
            path.relative_to(SUITE_ROOT).as_posix() for path in module_paths
        }
        self.assertEqual(cmake_modules, filesystem_modules)

        header = (
            SUITE_ROOT / "host" / "include" / "nsc_suite" / "TestRegistry.h"
        ).read_text(encoding="utf-8")
        main = (SUITE_ROOT / "host" / "src" / "main.cpp").read_text(
            encoding="utf-8"
        )
        declarations = set(REGISTER_DECLARATION.findall(header))
        calls = set(REGISTER_CALL.findall(main))
        self.assertEqual(declarations, set(definitions))
        self.assertEqual(calls, set(definitions))

    def test_target_sources_and_ctest_controllers_exist_exactly_once(self) -> None:
        cmake = (SUITE_ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        executable = self.executable_block(cmake)
        source_tokens = re.findall(
            r'(?:"([^"\n]+\.cpp)"|(?m:^\s*([A-Za-z0-9_./\\-]+\.cpp)\s*$))',
            executable,
        )
        sources: list[Path] = []
        for quoted, plain in source_tokens:
            token = quoted or plain
            prefix = "${CMAKE_CURRENT_LIST_DIR}/"
            if token.startswith(prefix):
                sources.append((SUITE_ROOT / token[len(prefix) :]).resolve())
            else:
                sources.append((SUITE_ROOT / token).resolve())
        self.assertTrue(sources)
        self.assertEqual(len(sources), len(set(sources)))
        self.assertEqual([path for path in sources if not path.is_file()], [])

        integration = SUITE_ROOT / "controller" / "integration"
        scripts = {path.name for path in integration.glob("run_*.py")}
        references = re.findall(
            r"controller/integration/(run_[A-Za-z0-9_]+\.py)", cmake
        )
        self.assertEqual(set(references), scripts)
        self.assertEqual(
            {name: count for name, count in Counter(references).items() if count != 1},
            {},
        )
        test_names = re.findall(r"\bNAME\s+([A-Za-z0-9_.-]+)", cmake)
        self.assertEqual(len(test_names), len(set(test_names)))


if __name__ == "__main__":
    unittest.main()
