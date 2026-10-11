from __future__ import annotations

import sys
import unittest
from pathlib import Path
from unittest.mock import patch


REPOSITORY_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPOSITORY_ROOT))

import nscbuilder3 as builder


TOOLS = {
    "cmake": "C:/apps/cmake/bin/cmake.exe",
    "ninja": "C:/apps/ninja/ninja.exe",
    "python": "C:/python/python.exe",
    "clang_cl": "C:/clang-llvm/bin/clang-cl.exe",
    "lld_link": "C:/clang-llvm/bin/lld-link.exe",
    "llvm_mt": "C:/clang-llvm/bin/llvm-mt.exe",
    "llvm_rc": "C:/clang-llvm/bin/llvm-rc.exe",
    "llvm_dir": "C:/clang-llvm/lib/cmake/llvm",
}


class Builder3CommandTests(unittest.TestCase):
    def setUp(self) -> None:
        self.config = builder.BuilderConfig(
            nsc_executable="A:/toolchain/bin/nsc.exe",
            nslink_executable="A:/toolchain/bin/nslink.exe",
            compiler_build_root="A:/nsc-devel/build-clang",
            install_prefix="A:/native-sharp-3.0-baseline",
            build_root="A:/nsc-devel/build-compiler-suite-v3",
            llvm_dir=TOOLS["llvm_dir"],
            jobs=12,
        )

    @patch.object(builder, "discover_build_tools", return_value=TOOLS)
    def test_test_infrastructure_configuration_isolated_and_normalized(
        self, _discover: object
    ) -> None:
        commands = builder.configure_commands(self.config, ("Debug", "Release"))

        self.assertEqual(
            [label for label, _, _ in commands],
            ["Configurar Debug", "Configurar Release"],
        )
        for mode, (_, command, cwd) in zip(("Debug", "Release"), commands):
            self.assertEqual(cwd, builder.SOURCE_ROOT)
            self.assertEqual(command[0], TOOLS["cmake"])
            self.assertIn(f"-DCMAKE_BUILD_TYPE={mode}", command)
            self.assertIn("-B", command)
            self.assertIn(
                f"A:/nsc-devel/build-compiler-suite-v3/{mode.casefold()}", command
            )
            self.assertIn(
                "-DNSC_COMPILER_SUITE_NSC_EXECUTABLE=A:/toolchain/bin/nsc.exe",
                command,
            )
            self.assertIn("-DPython3_EXECUTABLE=C:/python/python.exe", command)
            self.assertIn("-DNSC_NSLINK_EXECUTABLE=A:/toolchain/bin/nslink.exe", command)
            self.assertIn(
                "-DNSC_COMPILER_SUITE_DEFAULT_WINDOWS_RUNTIME_LIBRARY="
                "A:/toolchain/generated/NativeSharp.Runtime.Win32.lib",
                command,
            )
            self.assertIn("-DLLVM_DIR=C:/clang-llvm/lib/cmake/llvm", command)
            self.assertIn("-DCMAKE_EXE_LINKER_FLAGS=", command)
            self.assertIn("-UNSC_COMPILER_SUITE_ARCHIVE_EXECUTABLE", command)
            self.assertIn("-UNSC_COMPILER_SUITE_COMPARISON_NSC_EXECUTABLE", command)
            self.assertIn("-UNSC_COMPILER_SUITE_BUILD_ID_HEADER", command)
            self.assertFalse(any("Program Files" in argument for argument in command))
            self.assertFalse(any("\\" in argument for argument in command[1:]))

    def test_suite_cmake_preserves_explicit_runtime_library_authority(self) -> None:
        cmake_source = (builder.SUITE_ROOT / "CMakeLists.txt").read_text(
            encoding="utf-8"
        )

        self.assertIn(
            "set(NSC_COMPILER_SUITE_DEFAULT_WINDOWS_RUNTIME_LIBRARY\n"
            "    \"${CMAKE_INSTALL_PREFIX}/lib/nativesharp/runtime/"
            "NativeSharp.Runtime.Win32.lib\"\n"
            "    CACHE FILEPATH \"Default Windows runtime library for "
            "compiler-suite product cases\")",
            cmake_source,
        )

    def test_suite_cmake_recomputes_llvm_c_artifacts_from_llvm_dir(self) -> None:
        cmake_source = (builder.SUITE_ROOT / "CMakeLists.txt").read_text(
            encoding="utf-8"
        )

        self.assertIn(
            "find_library(NSC_COMPILER_SUITE_LLVM_C_IMPORT_LIBRARY\n"
            "    NAMES LLVM-C\n"
            "    HINTS \"${_nsc_suite_llvm_library_dir}\"\n"
            "    NO_DEFAULT_PATH\n"
            "    NO_CACHE\n"
            "    REQUIRED)",
            cmake_source,
        )
        self.assertIn(
            "find_file(NSC_COMPILER_SUITE_LLVM_C_RUNTIME\n"
            "    NAMES LLVM-C.dll\n"
            "    HINTS \"${_nsc_suite_llvm_root}/bin\"\n"
            "    NO_DEFAULT_PATH\n"
            "    NO_CACHE\n"
            "    REQUIRED)",
            cmake_source,
        )

    @patch.object(builder, "discover_build_tools", return_value=TOOLS)
    def test_test_infrastructure_build_targets_both_host_boundaries(
        self, _discover: object
    ) -> None:
        commands = builder.build_commands(self.config, ("Debug", "Release"))

        for mode, (_, command, _) in zip(("Debug", "Release"), commands):
            self.assertEqual(
                command,
                [
                    TOOLS["cmake"],
                    "--build",
                    f"A:/nsc-devel/build-compiler-suite-v3/{mode.casefold()}",
                    "--config",
                    mode,
                    "--target",
                    "nsc_component_tests",
                    "nsc_component_tests_destructive",
                    "nsc_component_tests_source_extended",
                    "--parallel",
                    "12",
                ],
            )

    @patch.object(builder, "discover_build_tools", return_value=TOOLS)
    def test_compiler_commands_propagate_mode_linkage_and_host_optimization(
        self, _discover: object
    ) -> None:
        configure = builder.compiler_configure_commands(
            self.config, ("Debug", "Release"), "Static", "O0"
        )
        build = builder.compiler_build_commands(self.config, ("Debug", "Release"))

        expected_roots = {
            "Debug": "A:/nsc-devel/build-clang-debug",
            "Release": "A:/nsc-devel/build-clang",
        }
        for mode, (_, command, _) in zip(("Debug", "Release"), configure):
            self.assertIn(expected_roots[mode], command)
            self.assertIn(f"-DCMAKE_BUILD_TYPE={mode}", command)
            self.assertIn("-DNSC_HOST_LINKAGE=STATIC", command)
            self.assertIn("-DNSC_HOST_OPTIMIZATION=O0", command)
            self.assertIn("-DNSC_BUILD_TESTS=OFF", command)
            self.assertIn("-DCMAKE_C_COMPILER=C:/clang-llvm/bin/clang-cl.exe", command)
            self.assertIn("-DCMAKE_CXX_COMPILER=C:/clang-llvm/bin/clang-cl.exe", command)
            self.assertIn("-DNSC_NSLINK_EXECUTABLE=A:/toolchain/bin/nslink.exe", command)
            self.assertFalse(any("\\" in argument for argument in command[1:]))
        for mode, (_, command, _) in zip(("Debug", "Release"), build):
            self.assertEqual(command[2], expected_roots[mode])
            self.assertEqual(command[command.index("--target") + 1], "nsc")
            self.assertEqual(command[command.index("--parallel") + 1], "12")

    @patch.object(builder, "discover_build_tools", return_value=TOOLS)
    def test_optional_inputs_replace_unset_cache_entries(self, _discover: object) -> None:
        configured = builder.BuilderConfig(
            **{
                **self.config.__dict__,
                "archive_executable": "A:/tools/7z.exe",
                "comparison_nsc": "A:/comparison/nsc.exe",
                "build_id_header": (
                    "A:/comparison/generated/nativesharp/Basic/BuildId.h"
                ),
            }
        )
        _, command, _ = builder.configure_commands(configured, ("Debug",))[0]

        self.assertIn("-DNSC_COMPILER_SUITE_ARCHIVE_EXECUTABLE=A:/tools/7z.exe", command)
        self.assertIn(
            "-DNSC_COMPILER_SUITE_COMPARISON_NSC_EXECUTABLE=A:/comparison/nsc.exe",
            command,
        )
        self.assertIn(
            "-DNSC_COMPILER_SUITE_BUILD_ID_HEADER="
            "A:/comparison/generated/nativesharp/Basic/BuildId.h",
            command,
        )
        self.assertIn(
            "-DNSC_COMPILER_SUITE_DEFAULT_WINDOWS_RUNTIME_LIBRARY="
            "A:/comparison/generated/NativeSharp.Runtime.Win32.lib",
            command,
        )
        self.assertFalse(
            any(argument.startswith("-UNSC_COMPILER_SUITE_") for argument in command)
        )


if __name__ == "__main__":
    unittest.main()
