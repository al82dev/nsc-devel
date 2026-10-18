from __future__ import annotations

import argparse
import re
from collections import defaultdict
from pathlib import Path


REGISTER_PATTERN = re.compile(r"\b(register[A-Za-z0-9_]+Module)\s*\(")
REGISTER_DEFINITION_PATTERN = re.compile(
    r"\bvoid\s+(register[A-Za-z0-9_]+Module)\s*\("
)
CPP_PATH_PATTERN = re.compile(r"[A-Za-z0-9_./${}-]+\.cpp")
PRODUCT_CPP_PATTERN = re.compile(r"(?<![A-Za-z0-9_])(src/[A-Za-z0-9_./-]+\.cpp)")
PRODUCT_INCLUDE_PATTERN = re.compile(
    r'^\s*#\s*include\s+"(?:\.\./)+(?P<path>src/[A-Za-z0-9_./-]+\.cpp)"',
    re.MULTILINE,
)


# These files are frozen Phase-1 descriptions, not executable tests. They either
# reference the retired InfrastructurePending disposition/helper or only record
# missing independent fixtures. Any new unconnected register*Module definition
# is rejected; expanding this list requires an explicit infrastructure decision.
INACTIVE_REGISTRATION_MODULES = {
    "host/modules/DriverInstallation433To496Module.cpp",
    "host/modules/DriverPipeline193To344Module.cpp",
    "host/modules/InteropCHeaderModule.cpp",
    "host/modules/LanguageServiceIndex161To232Module.cpp",
    "host/modules/LlvmGenericPartitionModule.cpp",
    "host/modules/LlvmOopAbiLifetimeModule.cpp",
    "host/modules/LlvmOopControlExpressionsModule.cpp",
    "host/modules/LlvmOopDataObjectsModule.cpp",
    "host/modules/LlvmOopRuntimeModule.cpp",
    "host/modules/NscProcess081To160Module.cpp",
    "host/modules/NscUnit001To080Module.cpp",
    "host/modules/RuntimeInfrastructureModule.cpp",
    "modules/LinkerFormatsModule.cpp",
    "modules/LinkerToolProcessModule.cpp",
}


def _without_comment(line: str) -> str:
    quoted = False
    escaped = False
    for index, character in enumerate(line):
        if escaped:
            escaped = False
        elif character == "\\":
            escaped = True
        elif character == '"':
            quoted = not quoted
        elif character == "#" and not quoted:
            return line[:index]
    return line


def _cmake_call(text: str, command: str, first_argument: str) -> str:
    start_match = re.search(
        rf"\b{re.escape(command)}\s*\(\s*{re.escape(first_argument)}\b", text
    )
    if start_match is None:
        raise RuntimeError(f"missing {command}({first_argument} ...) CMake declaration")
    start = text.find("(", start_match.start())
    depth = 0
    quoted = False
    escaped = False
    for index in range(start, len(text)):
        character = text[index]
        if escaped:
            escaped = False
            continue
        if character == "\\":
            escaped = True
            continue
        if character == '"':
            quoted = not quoted
            continue
        if quoted:
            continue
        if character == "#":
            newline = text.find("\n", index)
            if newline < 0:
                break
            continue
        if character == "(":
            depth += 1
        elif character == ")":
            depth -= 1
            if depth == 0:
                return text[start + 1 : index]
    raise RuntimeError(f"unterminated {command}({first_argument} ...) CMake declaration")


def _normalized_paths(paths: list[str] | tuple[str, ...] | set[str]) -> set[str]:
    return {path.replace("\\", "/") for path in paths}


def _module_sources(cmake_text: str) -> set[str]:
    return _normalized_paths(
        match.group(0)
        for match in CPP_PATH_PATTERN.finditer(cmake_text)
        if match.group(0).startswith(("host/modules/", "modules/"))
    )


def _definitions_by_source(suite_root: Path) -> dict[str, list[str]]:
    definitions: dict[str, list[str]] = defaultdict(list)
    for relative_root in (Path("host/modules"), Path("modules")):
        for path in sorted((suite_root / relative_root).glob("*.cpp")):
            relative = path.relative_to(suite_root).as_posix()
            text = path.read_text(encoding="utf-8-sig")
            for match in REGISTER_DEFINITION_PATTERN.finditer(text):
                definitions[match.group(1)].append(relative)
    return dict(definitions)


def _assert_equal(label: str, actual: set[str], expected: set[str]) -> None:
    missing = sorted(expected - actual)
    unexpected = sorted(actual - expected)
    if missing or unexpected:
        raise RuntimeError(
            f"{label} differs; missing={missing}; unexpected={unexpected}"
        )


def validate(repository_root: Path, suite_root: Path) -> None:
    root_cmake_path = repository_root / "CMakeLists.txt"
    suite_cmake_path = suite_root / "CMakeLists.txt"
    root_cmake = root_cmake_path.read_text(encoding="utf-8-sig")
    suite_cmake = suite_cmake_path.read_text(encoding="utf-8-sig")

    frontend_body = _cmake_call(root_cmake, "add_library", "nsc_frontend")
    product_sources = _normalized_paths(PRODUCT_CPP_PATTERN.findall(frontend_body))
    component_body = _cmake_call(
        suite_cmake, "add_executable", "nsc_component_tests"
    )
    covered_product_sources = _normalized_paths(
        PRODUCT_CPP_PATTERN.findall(component_body)
    )
    listed_component_sources = _module_sources(component_body)
    for relative in listed_component_sources:
        path = suite_root / relative
        if not path.is_file():
            raise RuntimeError(f"component test source does not exist: {relative}")
        covered_product_sources.update(
            _normalized_paths(PRODUCT_INCLUDE_PATTERN.findall(
                path.read_text(encoding="utf-8-sig")
            ))
        )
    missing_product_sources = sorted(product_sources - covered_product_sources)
    if missing_product_sources:
        raise RuntimeError(
            "nsc_component_tests omits nsc_frontend sources: "
            + ", ".join(missing_product_sources)
        )

    unit_sources = {
        path.relative_to(repository_root).as_posix()
        for path in (repository_root / "tests/unit").glob("*.cpp")
    }
    registered_unit_sources = _normalized_paths(
        re.findall(r"tests/unit/[A-Za-z0-9_./-]+\.cpp", root_cmake)
    )
    _assert_equal("unit-test CMake registration", registered_unit_sources, unit_sources)

    definitions = _definitions_by_source(suite_root)
    header_text = (
        suite_root / "host/include/nsc_suite/TestRegistry.h"
    ).read_text(encoding="utf-8-sig")
    main_text = (suite_root / "host/src/main.cpp").read_text(encoding="utf-8-sig")
    declarations = set(REGISTER_DEFINITION_PATTERN.findall(header_text))
    calls = set(REGISTER_PATTERN.findall(main_text))
    defined = set(definitions)
    active = declarations | calls
    if declarations != calls or active - defined:
        raise RuntimeError(
            "registry declaration/call/definition mismatch; "
            f"declared_not_called={sorted(declarations - calls)}; "
            f"called_not_declared={sorted(calls - declarations)}; "
            f"active_without_definition={sorted(active - defined)}"
        )
    duplicate_definitions = {
        name: sources for name, sources in definitions.items() if len(sources) != 1
    }
    if duplicate_definitions:
        raise RuntimeError(
            f"registry functions have duplicate definitions: {duplicate_definitions}"
        )
    all_cmake_modules = _module_sources(suite_cmake)
    unbuilt_active = sorted(
        name
        for name in active
        if definitions[name][0] not in all_cmake_modules
    )
    if unbuilt_active:
        raise RuntimeError(
            "active registry modules are absent from CMake: " + ", ".join(unbuilt_active)
        )
    inactive_sources = {
        sources[0] for name, sources in definitions.items() if name not in active
    }
    _assert_equal(
        "frozen inactive registration-module allowlist",
        inactive_sources,
        INACTIVE_REGISTRATION_MODULES,
    )

    for cmake_path, cmake_text, base in (
        (root_cmake_path, root_cmake, repository_root),
        (suite_cmake_path, suite_cmake, suite_root),
    ):
        for line_number, line in enumerate(cmake_text.splitlines(), start=1):
            source_line = _without_comment(line)
            for match in CPP_PATH_PATTERN.finditer(source_line):
                raw = match.group(0)
                if "/" not in raw and "\\" not in raw:
                    # A bare source name can be relative to a generated/subdirectory
                    # scope that this repository-level validator cannot resolve.
                    continue
                if "${" in raw:
                    product_match = PRODUCT_CPP_PATTERN.search(raw)
                    if product_match is None:
                        continue
                    path = repository_root / product_match.group(1)
                else:
                    path = base / raw
                if not path.is_file():
                    raise RuntimeError(
                        f"{cmake_path}:{line_number} references missing source {raw}"
                    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repository-root", type=Path, required=True)
    parser.add_argument("--suite-root", type=Path, required=True)
    options = parser.parse_args()
    validate(options.repository_root.resolve(), options.suite_root.resolve())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
