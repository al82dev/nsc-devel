from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory
from unittest.mock import patch


REPOSITORY_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPOSITORY_ROOT))

import nscbuilder3 as builder


class Builder3ConfigPersistenceTests(unittest.TestCase):
    def test_jobs_range_is_closed_during_payload_parsing(self) -> None:
        self.assertEqual(builder.parse_builder_config_payload({"jobs": 1}), {"jobs": 1})
        self.assertEqual(
            builder.parse_builder_config_payload({"jobs": 256}), {"jobs": 256}
        )
        for jobs in (-1, 0, 257, 1000):
            with self.subTest(jobs=jobs):
                with self.assertRaisesRegex(ValueError, "between 1 and 256"):
                    builder.parse_builder_config_payload({"jobs": jobs})

    def test_load_rejects_out_of_range_jobs_before_discovery(self) -> None:
        builder.STORAGE_ROOT.mkdir(parents=True, exist_ok=True)
        with TemporaryDirectory(dir=builder.STORAGE_ROOT) as directory:
            config_path = Path(directory) / "config.json"
            config_path.write_text('{"jobs":0}\n', encoding="utf-8")
            with patch.object(builder, "CONFIG_PATH", config_path), patch.object(
                builder.BuilderConfig, "with_discovery", autospec=True
            ) as discovery:
                with self.assertRaisesRegex(ValueError, "between 1 and 256"):
                    builder.BuilderConfig.load()

            discovery.assert_not_called()

    def test_failed_atomic_replace_preserves_previous_config_and_cleans_temp(self) -> None:
        builder.STORAGE_ROOT.mkdir(parents=True, exist_ok=True)
        with TemporaryDirectory(dir=builder.STORAGE_ROOT) as directory:
            root = Path(directory)
            config_path = root / "config.json"
            original = '{"jobs":4}\n'
            config_path.write_text(original, encoding="utf-8")
            with patch.object(builder, "CONFIG_PATH", config_path), patch.object(
                builder.os, "replace", side_effect=OSError("replace failed")
            ):
                with self.assertRaisesRegex(OSError, "replace failed"):
                    builder.BuilderConfig(jobs=8).save()

            self.assertEqual(config_path.read_text(encoding="utf-8"), original)
            self.assertEqual(list(root.glob("*.tmp")), [])

    def test_successful_save_round_trips_without_temporary_files(self) -> None:
        builder.STORAGE_ROOT.mkdir(parents=True, exist_ok=True)
        with TemporaryDirectory(dir=builder.STORAGE_ROOT) as directory:
            root = Path(directory)
            config_path = root / "config.json"
            expected = builder.BuilderConfig(jobs=12)
            with patch.object(builder, "CONFIG_PATH", config_path):
                expected.save()
                payload = json.loads(config_path.read_text(encoding="utf-8"))
                with patch.object(
                    builder.BuilderConfig,
                    "with_discovery",
                    autospec=True,
                    side_effect=lambda value: value,
                ):
                    actual = builder.BuilderConfig.load()

            self.assertEqual(actual, expected)
            self.assertEqual(payload, builder.asdict(expected))
            self.assertEqual(list(root.glob("*.tmp")), [])


if __name__ == "__main__":
    unittest.main()
