from __future__ import annotations

import sys
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPOSITORY_ROOT))

import nscbuilder3 as builder


class Builder3AllocationDomainTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        components = {
            row["component"]
            for row in builder.read_catalog_csv(
                builder.WAVES_PATH, builder.WAVE_CATALOG_FIELDS
            )
        }
        cls.rows = [
            row
            for row in builder.read_catalog_csv(
                builder.ALLOCATION_PATH, builder.ALLOCATION_CATALOG_FIELDS
            )
            if row["component"] in components
        ]

    def test_live_compiler_allocation_domains_are_supported(self) -> None:
        builder.validate_allocation_domains(self.rows)

    def test_unknown_scalar_domains_are_rejected_by_field(self) -> None:
        mutations = {
            "layer": "future-layer",
            "planned_executor": "future-runner",
            "default_profile": "SOMETIMES",
            "isolation": "SHARED_GLOBAL_STATE",
        }
        for field, value in mutations.items():
            with self.subTest(field=field):
                row = dict(self.rows[0])
                row[field] = value
                with self.assertRaisesRegex(ValueError, field):
                    builder.validate_allocation_domains([row])

    def test_unknown_mode_and_optimization_tokens_are_rejected(self) -> None:
        mutations = {
            "supported_build_modes": "Debug;Profile",
            "supported_optimizations": "O0;Ofast",
        }
        for field, value in mutations.items():
            with self.subTest(field=field):
                row = dict(self.rows[0])
                row[field] = value
                with self.assertRaisesRegex(ValueError, field):
                    builder.validate_allocation_domains([row])

    def test_sentinel_values_cannot_be_mixed_with_concrete_values(self) -> None:
        mutations = (
            ("supported_build_modes", "FROZEN_CONTRACT;Debug"),
            ("supported_optimizations", "N/A;O0"),
            ("supported_build_modes", "Debug;Debug"),
            ("supported_optimizations", "O0;O0"),
        )
        for field, value in mutations:
            with self.subTest(field=field, value=value):
                row = dict(self.rows[0])
                row[field] = value
                with self.assertRaisesRegex(ValueError, field):
                    builder.validate_allocation_domains([row])


if __name__ == "__main__":
    unittest.main()
