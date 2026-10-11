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
from nsc_builder.build import (
    clean_compiler_build_trees,
    compiler_build_cleanup_targets,
    compiler_cleanup_invalidated_fields,
)
from nsc_builder.paths import SOURCE_ROOT


def configuration(root: Path) -> SimpleNamespace:
    tools = root / "tools"
    tools.mkdir(parents=True, exist_ok=True)
    nslink = tools / "nslink.exe"
    nslink.write_bytes(b"nslink")
    llvm = root / "llvm"
    llvm.mkdir(exist_ok=True)
    return SimpleNamespace(
        compiler_build_root=str(root / "compiler-release"),
        build_root=str(root / "test-builds"),
        install_prefix=str(root / "installation"),
        nsc_executable=str(root / "compiler-release" / "bin" / "nsc.exe"),
        nslink_executable=str(nslink),
        comparison_nsc=str(root / "compiler-release-debug" / "bin" / "nsc.exe"),
        build_id_header=str(root / "compiler-release" / "generated" / "BuildId.h"),
        llvm_root_path=lambda: llvm,
    )


def create_generated_tree(path: Path, payload_name: str = "artifact.obj") -> None:
    path.mkdir(parents=True, exist_ok=True)
    (path / "CMakeCache.txt").write_text(
        f"CMAKE_HOME_DIRECTORY:INTERNAL={SOURCE_ROOT.as_posix()}\n",
        encoding="utf-8",
    )
    (path / "build.ninja").write_text("# generated\n", encoding="utf-8")
    nested = path / "objects" / "nested"
    nested.mkdir(parents=True)
    (nested / payload_name).write_bytes(b"generated")


class CompilerCleanupTests(unittest.TestCase):
    def test_targets_are_exact_debug_and_release_build_trees(self) -> None:
        with tempfile.TemporaryDirectory(dir=MODULAR_ROOT) as directory:
            root = Path(directory)
            config = configuration(root)

            targets = compiler_build_cleanup_targets(config)

            self.assertEqual(
                targets,
                (
                    (root / "compiler-release-debug").resolve(),
                    (root / "compiler-release").resolve(),
                ),
            )

    def test_cleanup_removes_only_compiler_trees(self) -> None:
        with tempfile.TemporaryDirectory(dir=MODULAR_ROOT) as directory:
            root = Path(directory)
            config = configuration(root)
            release = Path(config.compiler_build_root)
            debug = release.parent / f"{release.name}-debug"
            create_generated_tree(release)
            create_generated_tree(debug)
            protected = {
                "source": root / "source-sentinel.txt",
                "installation": Path(config.install_prefix) / "installed.txt",
                "tests": Path(config.build_root) / "test.obj",
                "history": root / "history" / "result.json",
                "configuration": root / "config.json",
            }
            for path in protected.values():
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("keep", encoding="utf-8")

            result = clean_compiler_build_trees(config)

            self.assertFalse(release.exists())
            self.assertFalse(debug.exists())
            self.assertFalse(result.failures)
            for label, path in protected.items():
                with self.subTest(label=label):
                    self.assertEqual(path.read_text(encoding="utf-8"), "keep")
            self.assertTrue(Path(config.nslink_executable).is_file())
            self.assertTrue(config.llvm_root_path().is_dir())

    def test_protected_and_non_generated_roots_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory(dir=MODULAR_ROOT) as directory:
            root = Path(directory)
            config = configuration(root)
            Path(config.install_prefix).mkdir(parents=True)
            config.compiler_build_root = config.install_prefix
            with self.assertRaisesRegex(ValueError, "protected path"):
                compiler_build_cleanup_targets(config)

            config = configuration(root)
            populated = Path(config.compiler_build_root)
            populated.mkdir(parents=True)
            (populated / "source.cpp").write_text("not generated", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "CMake/Ninja identity"):
                compiler_build_cleanup_targets(config)

    def test_source_root_and_reparse_target_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory(dir=MODULAR_ROOT) as directory:
            config = configuration(Path(directory))
            config.compiler_build_root = str(SOURCE_ROOT)
            with self.assertRaisesRegex(ValueError, "source tree"):
                compiler_build_cleanup_targets(config)

            config = configuration(Path(directory))
            release = Path(config.compiler_build_root).resolve()
            release.mkdir(parents=True)
            with patch(
                "nsc_builder.build._has_reparse_attribute",
                side_effect=lambda path: Path(path).resolve() == release,
            ):
                with self.assertRaisesRegex(ValueError, "symbolic link or junction"):
                    compiler_build_cleanup_targets(config)

    def test_cleanup_invalidates_only_artifacts_inside_compiler_trees(self) -> None:
        with tempfile.TemporaryDirectory(dir=MODULAR_ROOT) as directory:
            root = Path(directory)
            config = configuration(root)
            targets = compiler_build_cleanup_targets(config)

            invalidated = compiler_cleanup_invalidated_fields(config, targets)

            self.assertEqual(
                invalidated,
                ("nsc_executable", "comparison_nsc", "build_id_header"),
            )
            self.assertNotIn("nslink_executable", invalidated)
            self.assertNotIn("install_prefix", invalidated)

    def test_partial_errors_are_enumerated(self) -> None:
        with tempfile.TemporaryDirectory(dir=MODULAR_ROOT) as directory:
            root = Path(directory)
            config = configuration(root)
            release = Path(config.compiler_build_root)
            debug = release.parent / f"{release.name}-debug"
            create_generated_tree(release, "blocked.obj")
            create_generated_tree(debug)
            blocked = release / "objects" / "nested" / "blocked.obj"
            original_unlink = Path.unlink

            def controlled_unlink(path: Path, *args: object, **kwargs: object) -> None:
                if path == blocked:
                    raise PermissionError("synthetic lock")
                original_unlink(path, *args, **kwargs)

            with patch.object(Path, "unlink", controlled_unlink):
                result = clean_compiler_build_trees(config)

            self.assertTrue(result.failures)
            self.assertTrue(any(str(blocked) in failure for failure in result.failures))
            self.assertFalse(debug.exists())


if __name__ == "__main__":
    unittest.main()
