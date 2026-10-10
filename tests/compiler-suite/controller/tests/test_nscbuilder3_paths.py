from __future__ import annotations

import ast
import hashlib
import sys
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory
from unittest.mock import patch


REPOSITORY_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPOSITORY_ROOT))

import nscbuilder3 as builder


class Builder3PathTests(unittest.TestCase):
    def test_build_tool_discovery_uses_current_python_interpreter(self) -> None:
        builder.STORAGE_ROOT.mkdir(parents=True, exist_ok=True)
        with TemporaryDirectory(dir=builder.STORAGE_ROOT) as directory:
            root = Path(directory)
            bin_root = root / "bin"
            llvm_dir = root / "lib" / "cmake" / "llvm"
            bin_root.mkdir(parents=True)
            llvm_dir.mkdir(parents=True)
            tools = {
                "cmake": root / "cmake.exe",
                "ninja": root / "ninja.exe",
                "clang-cl": bin_root / "clang-cl.exe",
            }
            for path in (
                *tools.values(),
                bin_root / "lld-link.exe",
                bin_root / "llvm-mt.exe",
                bin_root / "llvm-rc.exe",
            ):
                path.write_bytes(b"tool\n")

            with patch.object(
                builder.shutil,
                "which",
                side_effect=lambda name: str(tools[name]) if name in tools else None,
            ):
                discovered = builder.discover_build_tools(str(llvm_dir))

            self.assertEqual(discovered["python"], str(Path(sys.executable).resolve()))

    def test_discovery_selectors_distinguish_files_from_directories(self) -> None:
        builder.STORAGE_ROOT.mkdir(parents=True, exist_ok=True)
        with TemporaryDirectory(dir=builder.STORAGE_ROOT) as directory:
            root = Path(directory)
            file_path = root / "candidate.exe"
            directory_path = root / "candidate-dir"
            file_path.write_bytes(b"tool\n")
            directory_path.mkdir()

            self.assertEqual(
                builder.first_existing_file([directory_path, file_path]),
                str(file_path.resolve()),
            )
            self.assertEqual(
                builder.first_existing_directory([file_path, directory_path]),
                str(directory_path.resolve()),
            )

    def test_discovery_selectors_return_empty_without_matching_kind(self) -> None:
        builder.STORAGE_ROOT.mkdir(parents=True, exist_ok=True)
        with TemporaryDirectory(dir=builder.STORAGE_ROOT) as directory:
            root = Path(directory)
            file_path = root / "candidate.exe"
            directory_path = root / "candidate-dir"
            file_path.write_bytes(b"tool\n")
            directory_path.mkdir()

            self.assertEqual(builder.first_existing_file([directory_path]), "")
            self.assertEqual(builder.first_existing_directory([file_path]), "")

    def test_operation_start_never_redetects_explicit_tool_paths(self) -> None:
        tree = ast.parse((REPOSITORY_ROOT / "nscbuilder3.py").read_text(encoding="utf-8"))
        methods = {
            node.name: node
            for node in ast.walk(tree)
            if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef))
        }

        start = methods["_start_infrastructure"]
        redetect = methods["_redetect_config"]
        start_calls = {
            node.func.attr
            for node in ast.walk(start)
            if isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute)
        }
        redetect_calls = {
            node.func.attr
            for node in ast.walk(redetect)
            if isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute)
        }

        self.assertNotIn("with_discovery", start_calls)
        self.assertIn("with_discovery", redetect_calls)

    def test_every_writable_root_must_remain_on_a_drive(self) -> None:
        baseline = builder.BuilderConfig(jobs=1)
        writable_fields = ("build_root", "compiler_build_root", "install_prefix")

        for field in writable_fields:
            with self.subTest(field=field):
                values = {**baseline.__dict__, field: f"C:/forbidden/{field}"}
                with self.assertRaisesRegex(ValueError, "must be on A:"):
                    builder.BuilderConfig(**values).validated(require_tools=False)

    def test_jobs_outside_supported_range_are_rejected(self) -> None:
        for jobs in (0, 257):
            with self.subTest(jobs=jobs):
                with self.assertRaisesRegex(ValueError, "between 1 and 256"):
                    builder.BuilderConfig(jobs=jobs).validated(require_tools=False)

    def test_existing_files_cannot_be_used_as_writable_roots(self) -> None:
        builder.STORAGE_ROOT.mkdir(parents=True, exist_ok=True)
        with TemporaryDirectory(dir=builder.STORAGE_ROOT) as directory:
            file_path = Path(directory) / "not-a-root"
            file_path.write_text("file\n", encoding="utf-8")
            baseline = builder.BuilderConfig(jobs=1)
            for field in ("build_root", "compiler_build_root", "install_prefix"):
                with self.subTest(field=field):
                    values = {**baseline.__dict__, field: str(file_path)}
                    with self.assertRaisesRegex(ValueError, "directory"):
                        builder.BuilderConfig(**values).validated(require_tools=False)

    def test_required_tools_must_be_files_not_existing_directories(self) -> None:
        builder.STORAGE_ROOT.mkdir(parents=True, exist_ok=True)
        with TemporaryDirectory(dir=builder.STORAGE_ROOT) as directory:
            root = Path(directory)
            valid = root / "tool.exe"
            valid.write_bytes(b"tool\n")
            for field in ("nsc_executable", "nslink_executable"):
                with self.subTest(field=field):
                    values = {
                        **builder.BuilderConfig(jobs=1).__dict__,
                        "nsc_executable": str(valid),
                        "nslink_executable": str(valid),
                        field: str(root),
                    }
                    with self.assertRaisesRegex(ValueError, "file"):
                        builder.BuilderConfig(**values).validated(require_tools=True)

    def test_optional_files_and_llvm_directory_have_exact_kinds(self) -> None:
        builder.STORAGE_ROOT.mkdir(parents=True, exist_ok=True)
        with TemporaryDirectory(dir=builder.STORAGE_ROOT) as directory:
            root = Path(directory)
            valid = root / "tool.exe"
            valid.write_bytes(b"tool\n")
            baseline = builder.BuilderConfig(
                nsc_executable=str(valid),
                nslink_executable=str(valid),
                jobs=1,
            )
            for field in ("archive_executable", "comparison_nsc", "build_id_header"):
                with self.subTest(field=field):
                    values = {**baseline.__dict__, field: str(root)}
                    with self.assertRaisesRegex(ValueError, "file"):
                        builder.BuilderConfig(**values).validated(require_tools=True)
            llvm_file = root / "LLVMConfig.cmake"
            llvm_file.write_text("config\n", encoding="utf-8")
            values = {**baseline.__dict__, "llvm_dir": str(llvm_file)}
            with self.assertRaisesRegex(ValueError, "directory"):
                builder.BuilderConfig(**values).validated(require_tools=True)

    def test_file_receipt_identifies_exact_selected_file(self) -> None:
        builder.STORAGE_ROOT.mkdir(parents=True, exist_ok=True)
        with TemporaryDirectory(dir=builder.STORAGE_ROOT) as directory:
            artifact = Path(directory) / "tool.exe"
            payload = b"selected-tool-identity\n"
            artifact.write_bytes(payload)

            receipt = builder.file_receipt("tool", artifact)

            self.assertEqual(receipt["name"], "tool")
            self.assertEqual(receipt["path"], str(artifact.resolve()))
            self.assertEqual(receipt["size"], len(payload))
            self.assertEqual(receipt["sha256"], hashlib.sha256(payload).hexdigest())

    def test_compiler_receipt_preserves_selected_nsc_and_nslink(self) -> None:
        builder.STORAGE_ROOT.mkdir(parents=True, exist_ok=True)
        with TemporaryDirectory(dir=builder.STORAGE_ROOT) as directory:
            root = Path(directory)
            nsc = root / "selected-nsc.exe"
            nslink = root / "selected-nslink.exe"
            nsc.write_bytes(b"nsc-selected\n")
            nslink.write_bytes(b"nslink-selected\n")
            config = builder.BuilderConfig(
                nsc_executable=str(nsc),
                nslink_executable=str(nslink),
                compiler_build_root="A:/nsc-devel/build-clang",
                install_prefix="A:/native-sharp-3.0-baseline",
                build_root="A:/nsc-devel/build-compiler-suite-v3",
                jobs=1,
            )

            receipt = builder.compiler_receipt(config)

            self.assertEqual(
                [(item["name"], item["path"]) for item in receipt["tools"]],
                [("nsc", str(nsc.resolve())), ("nslink", str(nslink.resolve()))],
            )


if __name__ == "__main__":
    unittest.main()
