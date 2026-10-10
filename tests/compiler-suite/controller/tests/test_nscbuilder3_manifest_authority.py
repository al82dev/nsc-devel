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


class Builder3ManifestAuthorityTests(unittest.TestCase):
    def test_live_manifest_passes_the_canonical_loader(self) -> None:
        loaded = builder.validate_executable_manifest()

        self.assertEqual(len(loaded), len(builder.read_jsonl(builder.MANIFEST_PATH)))

    def test_manifest_scalar_fields_are_never_coerced(self) -> None:
        source = builder.read_jsonl(builder.MANIFEST_PATH)[0]
        mutations = {
            "case_id": 7,
            "executor": 7,
            "expected_outcome": True,
            "optimizations": [0],
        }
        builder.STORAGE_ROOT.mkdir(parents=True, exist_ok=True)
        with TemporaryDirectory(dir=builder.STORAGE_ROOT) as directory:
            root = Path(directory)
            for field, value in mutations.items():
                with self.subTest(field=field):
                    payload = dict(source)
                    payload[field] = value
                    manifest_path = root / f"{field}.jsonl"
                    manifest_path.write_text(
                        json.dumps(payload, sort_keys=True) + "\n", encoding="utf-8"
                    )
                    with self.assertRaisesRegex(ValueError, field):
                        builder.validate_executable_manifest(manifest_path=manifest_path)

    def test_catalog_validates_before_reading_its_projection(self) -> None:
        validated = builder.validate_executable_manifest()
        with patch.object(
            builder, "validate_executable_manifest", return_value=validated
        ) as validator:
            builder.CanonicalCatalog.load()

        validator.assert_called_once_with()

    def test_executable_projection_uses_the_validated_manifest_fields(self) -> None:
        expected = {
            case.case_id: case for case in builder.validate_executable_manifest()
        }
        catalog = builder.CanonicalCatalog.load()

        for actual in (case for case in catalog.cases if case.executable):
            canonical = expected[actual.case_id]
            with self.subTest(case_id=actual.case_id):
                self.assertEqual(actual.layer, canonical.layer)
                self.assertEqual(actual.profile, canonical.profile)
                self.assertEqual(actual.build_modes, canonical.build_modes)
                self.assertEqual(actual.optimizations, canonical.optimizations)
                self.assertEqual(actual.executor, canonical.executor)
                self.assertEqual(actual.expected_outcome, canonical.expected_outcome)


if __name__ == "__main__":
    unittest.main()
