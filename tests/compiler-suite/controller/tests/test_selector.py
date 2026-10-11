from __future__ import annotations

import sys
import unittest
from pathlib import Path


CONTROLLER_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(CONTROLLER_ROOT))

from nsc_suite.manifest import CaseManifest
from nsc_suite.selector import Selection, SelectionError, select_cases


def case(
    case_id: str,
    *,
    contract_id: str = "CB-BAS-001",
    component: str = "Host.Basic",
    profile: str = "PR",
    build_modes: tuple[str, ...] = ("Debug", "Release"),
    optimizations: tuple[str, ...] = ("NA",),
    dependencies: tuple[str, ...] = (),
    tags: tuple[str, ...] = ("foundation",),
) -> CaseManifest:
    return CaseManifest(
        case_id=case_id,
        contract_id=contract_id,
        component=component,
        case_class="nominal",
        executor="nsc_component_tests",
        layer="unit",
        inputs={},
        oracle={"value": 1},
        dependencies=dependencies,
        profile=profile,
        build_modes=build_modes,
        optimizations=optimizations,
        isolation="in-process-reset",
        tags=tags,
    )


class SelectorTests(unittest.TestCase):
    def setUp(self) -> None:
        self.cases = (
            case("TB-BAS-001"),
            case(
                "TB-BAS-002",
                profile="NIGHTLY",
                dependencies=("TB-BAS-001",),
                tags=("foundation", "slow"),
            ),
            case(
                "TB-SRC-001",
                contract_id="CB-SRC-001",
                component="Host.Source",
                profile="WEEKLY",
                build_modes=("Release",),
                tags=("source",),
            ),
        )

    def test_requires_explicit_all_or_filter(self) -> None:
        with self.assertRaisesRegex(SelectionError, "requires --all"):
            select_cases(self.cases, Selection())
        with self.assertRaisesRegex(SelectionError, "identity or profile"):
            select_cases(self.cases, Selection(all_cases=True, profile="PR"))

    def test_all_is_deterministic(self) -> None:
        selected = select_cases(reversed(self.cases), Selection(all_cases=True))
        self.assertEqual(
            [item.case_id for item in selected],
            ["TB-BAS-001", "TB-BAS-002", "TB-SRC-001"],
        )

    def test_all_accepts_orthogonal_build_configuration(self) -> None:
        selected = select_cases(
            self.cases,
            Selection(all_cases=True, build_mode="Debug", optimization="NA"),
        )
        self.assertEqual(
            [item.case_id for item in selected],
            ["TB-BAS-001", "TB-BAS-002"],
        )

    def test_filters_intersect(self) -> None:
        selected = select_cases(
            self.cases,
            Selection(
                components=frozenset({"Host.Basic"}),
                tags=frozenset({"slow"}),
                build_mode="Debug",
            ),
        )
        self.assertEqual([item.case_id for item in selected], ["TB-BAS-001", "TB-BAS-002"])
        self.assertEqual(selected[-1].case_id, "TB-BAS-002")

    def test_profile_is_cumulative(self) -> None:
        pr = select_cases(self.cases, Selection(profile="PR"))
        nightly = select_cases(self.cases, Selection(profile="NIGHTLY"))
        weekly = select_cases(self.cases, Selection(profile="WEEKLY"))
        self.assertEqual([item.case_id for item in pr], ["TB-BAS-001"])
        self.assertEqual([item.case_id for item in nightly], ["TB-BAS-001", "TB-BAS-002"])
        self.assertEqual(len(weekly), 3)

    def test_dependency_closure_adds_producers(self) -> None:
        selected = select_cases(
            self.cases, Selection(case_ids=frozenset({"TB-BAS-002"}))
        )
        self.assertEqual([item.case_id for item in selected], ["TB-BAS-001", "TB-BAS-002"])
        without_closure = select_cases(
            self.cases,
            Selection(
                case_ids=frozenset({"TB-BAS-002"}), include_dependencies=False
            ),
        )
        self.assertEqual([item.case_id for item in without_closure], ["TB-BAS-002"])

    def test_dependency_closure_preserves_external_producers_without_selecting_them(self) -> None:
        consumer = case(
            "TB-BAS-003",
            dependencies=("TB-BAS-001", "CMake", "LLVM 23", "NSLink"),
        )
        selected = select_cases(
            (*self.cases, consumer),
            Selection(case_ids=frozenset({"TB-BAS-003"})),
        )
        self.assertEqual(
            [item.case_id for item in selected],
            ["TB-BAS-001", "TB-BAS-003"],
        )
        self.assertEqual(
            selected[-1].dependencies,
            ("TB-BAS-001", "CMake", "LLVM 23", "NSLink"),
        )

    def test_dependency_closure_still_rejects_missing_case_ids(self) -> None:
        consumer = case("TB-BAS-003", dependencies=("TB-BAS-999",))
        with self.assertRaisesRegex(SelectionError, "missing dependency TB-BAS-999"):
            select_cases(
                (*self.cases, consumer),
                Selection(case_ids=frozenset({"TB-BAS-003"})),
            )

    def test_empty_and_unknown_values_are_errors(self) -> None:
        with self.assertRaisesRegex(SelectionError, "unknown case selection"):
            select_cases(
                self.cases, Selection(case_ids=frozenset({"TB-BAS-999"}))
            )
        with self.assertRaisesRegex(SelectionError, "unknown profile"):
            select_cases(self.cases, Selection(profile="DAILY"))

    def test_rejects_partially_unknown_union_filters(self) -> None:
        selections = (
            ("case", Selection(case_ids=frozenset({"TB-BAS-001", "TB-BAS-999"}))),
            (
                "contract",
                Selection(contract_ids=frozenset({"CB-BAS-001", "CB-NONE-999"})),
            ),
            (
                "component",
                Selection(components=frozenset({"Host.Basic", "Host.Missing"})),
            ),
            ("layer", Selection(layers=frozenset({"unit", "missing-layer"}))),
        )
        for field, selection in selections:
            with self.subTest(field=field):
                with self.assertRaisesRegex(
                    SelectionError, f"unknown {field} selection"
                ):
                    select_cases(self.cases, selection)


if __name__ == "__main__":
    unittest.main()
