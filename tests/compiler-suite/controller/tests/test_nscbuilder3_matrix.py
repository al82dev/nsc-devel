from __future__ import annotations

import sys
import unittest
from collections import defaultdict
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPOSITORY_ROOT))

import nscbuilder3 as builder


class Builder3MatrixTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.catalog = builder.CanonicalCatalog.load()

    def test_every_scope_partition_matches_live_case_fields(self) -> None:
        attributes = {
            "Ola": lambda case: str(case.wave),
            "Componente": lambda case: case.component,
            "Contrato": lambda case: case.contract_id,
        }
        for scope, key_of in attributes.items():
            expected: dict[str, list[str]] = defaultdict(list)
            for case in self.catalog.cases:
                expected[key_of(case)].append(case.case_id)
            for value, case_ids in expected.items():
                selected = self.catalog.select(scope, value, "Todos")
                self.assertEqual([case.case_id for case in selected], case_ids)

        for case in (
            self.catalog.cases[0],
            self.catalog.cases[len(self.catalog.cases) // 2],
            self.catalog.cases[-1],
        ):
            self.assertEqual(
                self.catalog.select("Caso", case.case_id, "Todos"),
                (case,),
            )

    def test_profiles_are_cumulative_over_the_complete_catalog(self) -> None:
        for profile in ("PR", "NIGHTLY", "WEEKLY"):
            expected = tuple(
                case
                for case in self.catalog.cases
                if builder.PROFILE_RANK.get(case.profile, 99)
                <= builder.PROFILE_RANK[profile]
            )
            self.assertEqual(self.catalog.select("Total", "", profile), expected)
        self.assertEqual(
            self.catalog.select("Total", "", "Todos"), self.catalog.cases
        )

    def test_complete_variant_matrix_is_exact_and_unique(self) -> None:
        actual = builder.expand_variants(self.catalog.cases, "Todos", "Todas")
        actual_keys = [variant.key for variant in actual]
        expected_keys: list[str] = []
        for case in self.catalog.cases:
            if not case.executable:
                continue
            optimizations = (
                ("NA",)
                if case.optimizations == ("NA",)
                else tuple(
                    value
                    for value in ("O0", "O1", "O2", "O3")
                    if value in case.optimizations
                )
            )
            for mode in ("Debug", "Release"):
                if mode not in case.build_modes:
                    continue
                expected_keys.extend(
                    f"{case.case_id}|{mode}|{optimization}"
                    for optimization in optimizations
                )

        self.assertEqual(actual_keys, expected_keys)
        self.assertEqual(len(actual_keys), len(set(actual_keys)))
        self.assertEqual(
            {variant.case.case_id for variant in actual},
            {case.case_id for case in self.catalog.cases if case.executable},
        )

    def test_active_catalog_contains_only_executable_cases(self) -> None:
        self.assertTrue(self.catalog.cases)
        self.assertTrue(all(case.executable for case in self.catalog.cases))
        self.assertEqual(
            {variant.case.case_id for variant in builder.expand_variants(
                self.catalog.cases, "Todos", "Todas"
            )},
            {case.case_id for case in self.catalog.cases},
        )


if __name__ == "__main__":
    unittest.main()
