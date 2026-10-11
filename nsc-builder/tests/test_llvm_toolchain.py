from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
MODULAR_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(MODULAR_ROOT))
sys.path.insert(0, str(REPOSITORY_ROOT))

import nscbuilder3 as monolith
from nsc_builder import toolchain as modular


IDENTITIES = {
    "clang-cl": "clang version 23.1.0",
    "lld-link": "LLD 23.1.0",
    "llvm-rc": "OVERVIEW: LLVM Resource Converter",
    "llvm-mt": "OVERVIEW: Manifest Tool",
}


def identity(path: Path) -> str:
    return IDENTITIES[path.stem.casefold()]


def create_toolchain(parent: Path, version: str, *, missing: str = "") -> Path:
    root = parent / f"llvm-{version}"
    binary = root / "bin"
    configuration = root / "lib" / "cmake" / "llvm"
    binary.mkdir(parents=True)
    configuration.mkdir(parents=True)
    for filename in ("clang-cl.exe", "lld-link.exe", "llvm-rc.exe", "llvm-mt.exe"):
        if filename != missing:
            (binary / filename).write_bytes(b"fixture")
    (configuration / "LLVMConfig.cmake").write_text(
        f'set(LLVM_PACKAGE_VERSION "{version}")\n', encoding="utf-8"
    )
    return root


class StrictLLVMToolchainTests(unittest.TestCase):
    def implementations(self):
        return (monolith, modular)

    def test_llvm_23_complete_root_is_accepted(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = create_toolchain(Path(temporary), "23.1.0")
            for implementation in self.implementations():
                with self.subTest(implementation=implementation.__name__):
                    receipt = implementation.validate_llvm_toolchain(
                        root, identity_reader=identity
                    )
                    self.assertEqual(Path(receipt["llvm_root"]), root.resolve())
                    self.assertEqual(receipt["llvm_version"], "23.1.0")
                    self.assertEqual(receipt["llvm_config_version"], "23.1.0")
                    for producer in implementation.LLVM_PRODUCER_NAMES:
                        self.assertEqual(receipt[f"{producer}_version"], "23.1.0")

    def test_llvm_22_root_is_rejected_before_cmake(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = create_toolchain(Path(temporary), "22.1.8")
            for implementation in self.implementations():
                with self.subTest(implementation=implementation.__name__):
                    with self.assertRaisesRegex(ValueError, "major 23.*22.1.8"):
                        implementation.validate_llvm_toolchain(
                            root, identity_reader=identity
                        )

    def test_mixed_producer_root_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            parent = Path(temporary)
            root = create_toolchain(parent, "23.1.0")
            other = create_toolchain(parent, "23.2.0")
            producers = {
                "clang_cl": root / "bin" / "clang-cl.exe",
                "lld_link": other / "bin" / "lld-link.exe",
                "llvm_rc": root / "bin" / "llvm-rc.exe",
                "llvm_mt": root / "bin" / "llvm-mt.exe",
            }
            for implementation in self.implementations():
                with self.subTest(implementation=implementation.__name__):
                    with self.assertRaisesRegex(ValueError, "mixed-root producer"):
                        implementation.validate_llvm_toolchain(
                            root,
                            producer_paths=producers,
                            identity_reader=identity,
                        )

    def test_missing_producer_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = create_toolchain(
                Path(temporary), "23.1.0", missing="llvm-mt.exe"
            )
            for implementation in self.implementations():
                with self.subTest(implementation=implementation.__name__):
                    with self.assertRaisesRegex(ValueError, "producer does not exist"):
                        implementation.validate_llvm_toolchain(
                            root, identity_reader=identity
                        )

    def test_explicit_root_replaces_a_stale_independent_llvm_dir(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            parent = Path(temporary)
            root = create_toolchain(parent, "23.1.0")
            stale = create_toolchain(parent, "22.1.8") / "lib" / "cmake" / "llvm"
            config = monolith.BuilderConfig(
                compiler_llvm_root=str(root), llvm_dir=str(stale)
            ).with_discovery()
            self.assertEqual(config.llvm_root_path(), root.resolve())
            self.assertEqual(
                Path(config.llvm_dir),
                root.resolve() / "lib" / "cmake" / "llvm",
            )


if __name__ == "__main__":
    unittest.main()
