from __future__ import annotations

import sys
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path
from tempfile import TemporaryDirectory
from types import SimpleNamespace
from unittest.mock import patch


REPOSITORY_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPOSITORY_ROOT))

import nscbuilder3 as builder


class Builder3FixtureFailureTests(unittest.TestCase):
    def _engine(self, root: Path) -> builder.ExecutionEngine:
        return builder.ExecutionEngine(
            builder.BuilderConfig(),
            SimpleNamespace(root=root),
            lambda _line: None,
            lambda *_args: None,
            lambda *_args: None,
        )

    def _variant(self) -> SimpleNamespace:
        return SimpleNamespace(
            key="TSH-00001|Release|O0", build_mode="Release", optimization="O0"
        )

    def test_timeout_and_cancellation_keep_their_process_cause(self) -> None:
        builder.STORAGE_ROOT.mkdir(parents=True, exist_ok=True)
        with TemporaryDirectory(dir=builder.STORAGE_ROOT) as directory:
            for reason, text in (("timeout", "timed out"), ("cancelled", "cancelled")):
                with self.subTest(reason=reason):
                    engine = self._engine(Path(directory) / reason)

                    def run(*_args: object, **_kwargs: object) -> tuple[int, str]:
                        engine.process.last_termination_reason = reason
                        return 130, "process evidence"

                    with patch.object(engine.process, "run", side_effect=run), patch.object(
                        engine, "_nsc_environment", return_value={}
                    ):
                        with self.assertRaisesRegex(RuntimeError, text):
                            engine._prepare_selfhost_fixture(
                                self._variant(), "selfhost/modules/SelfHostBasic.nsproj"
                            )

    def test_exit_failure_and_missing_artifact_are_distinct(self) -> None:
        builder.STORAGE_ROOT.mkdir(parents=True, exist_ok=True)
        with TemporaryDirectory(dir=builder.STORAGE_ROOT) as directory:
            cases = ((7, "exited with code 7"), (0, "without executable output"))
            for code, text in cases:
                with self.subTest(code=code):
                    engine = self._engine(Path(directory) / str(code))
                    with patch.object(
                        engine.process, "run", return_value=(code, "compiler evidence")
                    ), patch.object(engine, "_nsc_environment", return_value={}):
                        with self.assertRaisesRegex(RuntimeError, text):
                            engine._prepare_selfhost_fixture(
                                self._variant(), "selfhost/modules/SelfHostBasic.nsproj"
                            )

    def test_producer_compiles_the_materialized_project(self) -> None:
        builder.STORAGE_ROOT.mkdir(parents=True, exist_ok=True)
        with TemporaryDirectory(dir=builder.STORAGE_ROOT) as directory:
            engine = self._engine(Path(directory))
            observed_project: list[Path] = []

            def run(
                command: list[str], *_args: object, **_kwargs: object
            ) -> tuple[int, str]:
                observed_project.append(Path(command[2]))
                output = Path(command[command.index("-o") + 1])
                output.write_bytes(b"fixture\n")
                return 0, ""

            with patch.object(engine.process, "run", side_effect=run), patch.object(
                engine, "_nsc_environment", return_value={}
            ):
                executable = engine._prepare_selfhost_fixture(
                    self._variant(), "selfhost/modules/SelfHostBasic.nsproj"
                )

            self.assertTrue(executable.is_file())
            self.assertEqual(len(observed_project), 1)
            self.assertIn("project", observed_project[0].parts)
            self.assertNotEqual(
                observed_project[0],
                builder.SUITE_ROOT / "selfhost/modules/SelfHostBasic.nsproj",
            )
            for item in ET.parse(observed_project[0]).getroot().iter("Compile"):
                self.assertNotIn("..", Path(item.get("Include", "")).parts)

    def test_producer_timeout_is_cached_as_the_downstream_blocker(self) -> None:
        builder.STORAGE_ROOT.mkdir(parents=True, exist_ok=True)
        with TemporaryDirectory(dir=builder.STORAGE_ROOT) as directory:
            engine = self._engine(Path(directory))
            case = builder.CanonicalCatalog.load().by_id["TSH-00001"]
            variant = builder.RunVariant(case, "Release", "O0")
            detail = "fixture SelfHostBasic.nsproj timed out after 900.0 seconds"
            with patch.object(
                engine, "_prepare_selfhost_fixture", side_effect=RuntimeError(detail)
            ):
                first = engine._run_selfhost(variant)
                second = engine._run_selfhost(variant)

            self.assertEqual(first[0], "INFRASTRUCTURE_ERROR")
            self.assertEqual(first[1], detail)
            self.assertEqual(second[0], "BLOCKED")
            self.assertIn(detail, second[1])


if __name__ == "__main__":
    unittest.main()
