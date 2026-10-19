from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch


MODULAR_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(MODULAR_ROOT))

from nsc_builder import build


def make_source(root: Path, name: str) -> Path:
    source = root / name
    source.mkdir(parents=True)
    (source / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.25)\n", encoding="utf-8"
    )
    return source


def fake_tools(root: Path) -> dict[str, str]:
    llvm = root / "llvm"
    llvm_bin = llvm / "bin"
    llvm_bin.mkdir(parents=True)
    (llvm_bin / "llvm-config.exe").write_bytes(b"llvm-config")
    return {
        "cmake": str(root / "tools" / "cmake.exe"),
        "ninja": str(root / "tools" / "ninja.exe"),
        "llvm_root": str(llvm),
        "llvm_dir": str(llvm / "lib" / "cmake" / "llvm"),
        "clang_cl": str(llvm_bin / "clang-cl.exe"),
        "lld_link": str(llvm_bin / "lld-link.exe"),
        "llvm_mt": str(llvm_bin / "llvm-mt.exe"),
        "llvm_rc": str(llvm_bin / "llvm-rc.exe"),
    }


class Win32SiblingToolchainTests(unittest.TestCase):
    def test_full_build_orders_exact_products_and_publishes_beside_nsc(self) -> None:
        with tempfile.TemporaryDirectory(dir=MODULAR_ROOT) as directory:
            root = Path(directory)
            nslink = make_source(root, "nslink-devel")
            nsasm = make_source(root, "nsasm-devel")
            nsdbg = make_source(root, "nsc-debugger")
            tools = fake_tools(root)
            config = SimpleNamespace(
                compiler_build_root=str(root / "nsc-build-release"),
                install_prefix=str(root / "installation"),
                nslink_executable=str(root / "baseline" / "bin" / "nslink.exe"),
                jobs=12,
                llvm_root_path=lambda: Path(tools["llvm_root"]),
            )

            with (
                patch.object(build, "NSLINK_SOURCE_ROOT", nslink),
                patch.object(build, "NSASM_SOURCE_ROOT", nsasm),
                patch.object(build, "NSDBG_SOURCE_ROOT", nsdbg),
                patch.object(build, "discover_compiler_build_tools", return_value=tools),
            ):
                commands = (
                    build.compiler_nslink_commands(config, ("Release",))
                    + build.compiler_configure_commands(
                        config,
                        ("Release",),
                        "Static",
                        "O0",
                        use_sibling_nslink=True,
                    )
                    + build.compiler_build_commands(config, ("Release",))
                    + build.compiler_post_build_sibling_commands(config, ("Release",))
                )
                baseline_configure = build.compiler_configure_commands(
                    config, ("Release",), "Static", "O0"
                )

            self.assertEqual(
                [label for label, _, _ in commands],
                [
                    "Configurar NSLink Release",
                    "Construir NSLink Release",
                    "Configurar NSC Release",
                    "Construir NSC Release",
                    "Configurar NSAsm Release",
                    "Construir NSAsm Release",
                    "Configurar NSDBG Release",
                    "Construir NSDBG Release",
                ],
            )
            output = (root / "nsc-build-release" / "bin").resolve().as_posix()
            joined = [" ".join(command) for _, command, _ in commands]
            self.assertIn(f"-DNSLINK_PRODUCT_OUTPUT_DIR={output}", joined[0])
            self.assertIn(f"-DNSC_NSLINK_EXECUTABLE={output}/nslink.exe", joined[2])
            self.assertIn(f"-DNSASM_PRODUCT_OUTPUT_DIR={output}", joined[4])
            self.assertIn(f"-DNSDBG_PRODUCT_OUTPUT_DIR={output}", joined[6])
            self.assertIn(f"-DNSDBG_NSASM_EXECUTABLE={output}/nsasm.exe", joined[6])
            self.assertIn(f"-DNSDBG_NSLINK_EXECUTABLE={output}/nslink.exe", joined[6])
            self.assertIn(f"-DNSDBG_NSC_EXECUTABLE={output}/nsc.exe", joined[6])
            self.assertIn("--target nslink", joined[1])
            self.assertIn("--target nsc", joined[3])
            self.assertIn("--target nsasm", joined[5])
            self.assertIn("--target nsdbg", joined[7])
            self.assertTrue(all("test" not in command.casefold() for command in joined[1::2]))
            self.assertIn(
                f"-DNSC_NSLINK_EXECUTABLE={(root / 'baseline' / 'bin' / 'nslink.exe').resolve().as_posix()}",
                baseline_configure[0][1],
            )

    def test_unknown_sibling_product_is_rejected(self) -> None:
        config = SimpleNamespace(compiler_build_root="A:/nsc-devel/build-clang")
        with self.assertRaisesRegex(ValueError, "unsupported NativeSharp sibling tool"):
            build.compiler_sibling_output(config, "Release", "other")


if __name__ == "__main__":
    unittest.main()
