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


class Builder3ConfigSchemaTests(unittest.TestCase):
    def test_valid_partial_payload_preserves_declared_types(self) -> None:
        self.assertEqual(
            builder.parse_builder_config_payload(
                {"nsc_executable": "A:/tool/nsc.exe", "jobs": 12}
            ),
            {"nsc_executable": "A:/tool/nsc.exe", "jobs": 12},
        )

    def test_payload_must_be_an_object_without_unknown_fields(self) -> None:
        with self.assertRaisesRegex(ValueError, "JSON object"):
            builder.parse_builder_config_payload([])
        with self.assertRaisesRegex(ValueError, "unknown fields"):
            builder.parse_builder_config_payload({"jobs": 1, "fallback": True})

    def test_path_fields_and_jobs_are_not_coerced(self) -> None:
        for field in builder.CONFIG_STRING_FIELDS:
            with self.subTest(field=field):
                with self.assertRaisesRegex(ValueError, field):
                    builder.parse_builder_config_payload({field: 1})
        for jobs in (True, "12", 12.0, None):
            with self.subTest(jobs=jobs):
                with self.assertRaisesRegex(ValueError, "jobs"):
                    builder.parse_builder_config_payload({"jobs": jobs})

    def test_load_uses_closed_schema_before_discovery(self) -> None:
        builder.STORAGE_ROOT.mkdir(parents=True, exist_ok=True)
        with TemporaryDirectory(dir=builder.STORAGE_ROOT) as directory:
            config_path = Path(directory) / "config.json"
            config_path.write_text(
                json.dumps({"jobs": "not-an-integer"}), encoding="utf-8"
            )
            with patch.object(builder, "CONFIG_PATH", config_path), patch.object(
                builder.BuilderConfig,
                "with_discovery",
                autospec=True,
                side_effect=lambda value: value,
            ):
                with self.assertRaisesRegex(ValueError, "jobs"):
                    builder.BuilderConfig.load()

    def test_load_rejects_duplicate_configuration_keys(self) -> None:
        builder.STORAGE_ROOT.mkdir(parents=True, exist_ok=True)
        with TemporaryDirectory(dir=builder.STORAGE_ROOT) as directory:
            config_path = Path(directory) / "config.json"
            config_path.write_text(
                '{"jobs":4,"jobs":12}\n',
                encoding="utf-8",
            )
            with patch.object(builder, "CONFIG_PATH", config_path), patch.object(
                builder.BuilderConfig, "with_discovery", autospec=True
            ) as discovery:
                with self.assertRaisesRegex(ValueError, "duplicate JSON object key: jobs"):
                    builder.BuilderConfig.load()
            discovery.assert_not_called()


if __name__ == "__main__":
    unittest.main()
