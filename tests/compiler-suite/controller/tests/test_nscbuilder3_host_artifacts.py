from __future__ import annotations

import sys
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory


REPOSITORY_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPOSITORY_ROOT))

import nscbuilder3 as builder


class Builder3HostArtifactTests(unittest.TestCase):
    def setUp(self) -> None:
        builder.STORAGE_ROOT.mkdir(parents=True, exist_ok=True)
        self.temporary = TemporaryDirectory(dir=builder.STORAGE_ROOT)
        root = Path(self.temporary.name)
        self.config = builder.BuilderConfig(
            compiler_build_root=str(root / "compiler-release"),
            build_root=str(root / "suite"),
            jobs=1,
        )

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def test_debug_and_release_suite_roots_are_disjoint(self) -> None:
        debug = builder.effective_build_dir(self.config, "Debug")
        release = builder.effective_build_dir(self.config, "Release")

        self.assertNotEqual(debug, release)
        self.assertEqual(debug.name, "debug")
        self.assertEqual(release.name, "release")

    def test_host_lookup_never_crosses_build_modes(self) -> None:
        debug_root = builder.effective_build_dir(self.config, "Debug")
        release_root = builder.effective_build_dir(self.config, "Release")
        debug_host = debug_root / "bin" / "Debug" / "nsc_component_tests.exe"
        release_host = release_root / "nsc_component_tests.exe"
        debug_host.parent.mkdir(parents=True)
        release_host.parent.mkdir(parents=True)
        debug_host.write_bytes(b"debug")
        release_host.write_bytes(b"release")

        self.assertEqual(builder.host_executable(self.config, "Debug"), debug_host)
        self.assertEqual(builder.host_executable(self.config, "Release"), release_host)

    def test_missing_host_returns_expected_path_in_requested_tree(self) -> None:
        expected = builder.effective_build_dir(self.config, "Debug") / "nsc_component_tests.exe"

        selected = builder.host_executable(self.config, "Debug")

        self.assertEqual(selected, expected)
        self.assertFalse(selected.exists())

    def test_compiler_build_products_are_isolated_by_mode(self) -> None:
        release = builder.compiler_build_dir(self.config, "Release")
        debug = builder.compiler_build_dir(self.config, "Debug")

        self.assertEqual(release.name, "compiler-release")
        self.assertEqual(debug.name, "compiler-release-debug")
        self.assertNotEqual(builder.compiler_output(self.config, "Release"), builder.compiler_output(self.config, "Debug"))


if __name__ == "__main__":
    unittest.main()
