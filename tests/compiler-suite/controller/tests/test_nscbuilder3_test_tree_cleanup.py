from __future__ import annotations

import inspect
import sys
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory


REPOSITORY_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPOSITORY_ROOT))

import nscbuilder3 as builder


class Builder3TestTreeCleanupTests(unittest.TestCase):
    def _config(self, workspace: Path) -> builder.BuilderConfig:
        return builder.BuilderConfig(
            nsc_executable=str(workspace / "install" / "bin" / "nsc.exe"),
            nslink_executable=str(workspace / "install" / "bin" / "nslink.exe"),
            compiler_build_root=str(workspace / "compiler-build"),
            install_prefix=str(workspace / "install"),
            build_root=str(workspace / "test-build"),
        )

    def test_cleanup_targets_are_only_debug_and_release_test_builds(self) -> None:
        with TemporaryDirectory(dir=REPOSITORY_ROOT) as directory:
            config = self._config(Path(directory))

            self.assertEqual(
                builder.test_tree_cleanup_targets(config),
                (
                    Path(config.build_root).resolve() / "debug",
                    Path(config.build_root).resolve() / "release",
                ),
            )

    def test_cleanup_removes_generated_trees_and_preserves_siblings(self) -> None:
        with TemporaryDirectory(dir=REPOSITORY_ROOT) as directory:
            workspace = Path(directory)
            config = self._config(workspace)
            debug_object = Path(config.build_root) / "debug" / "obj" / "case.obj"
            release_cache = Path(config.build_root) / "release" / "CMakeCache.txt"
            preserved = workspace / "source.ns"
            debug_object.parent.mkdir(parents=True)
            release_cache.parent.mkdir(parents=True)
            debug_object.write_bytes(b"object")
            release_cache.write_text("cache\n", encoding="utf-8")
            preserved.write_text("function main() {}\n", encoding="utf-8")

            result = builder.clean_test_build_trees(config)

            self.assertFalse((Path(config.build_root) / "debug").exists())
            self.assertFalse((Path(config.build_root) / "release").exists())
            self.assertTrue(preserved.is_file())
            self.assertEqual(result.removed_files, 2)
            self.assertFalse(result.failures)

    def test_cleanup_rejects_source_and_compiler_tree_overlaps(self) -> None:
        source_config = self._config(REPOSITORY_ROOT)
        source_config = builder.BuilderConfig(
            **{**source_config.__dict__, "build_root": str(builder.SUITE_ROOT)}
        )
        with self.assertRaisesRegex(ValueError, "protected path"):
            builder.test_tree_cleanup_targets(source_config)

        with TemporaryDirectory(dir=REPOSITORY_ROOT) as directory:
            workspace = Path(directory)
            config = self._config(workspace)
            config = builder.BuilderConfig(
                **{
                    **config.__dict__,
                    "compiler_build_root": str(
                        Path(config.build_root).resolve() / "release"
                    ),
                }
            )
            with self.assertRaisesRegex(ValueError, "protected path"):
                builder.test_tree_cleanup_targets(config)

    def test_cleanup_uses_an_iterative_walk_without_recursive_delete(self) -> None:
        source = inspect.getsource(builder.clean_test_build_trees)
        self.assertIn("while stack", source)
        self.assertNotIn("rmtree", source)
        self.assertNotIn("recursive", source)


if __name__ == "__main__":
    unittest.main()
