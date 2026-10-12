from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch


MODULAR_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(MODULAR_ROOT))

from nsc_builder.build import configure_commands
from nsc_builder.catalog import CanonicalCatalog, refresh_run_variants
from nsc_builder.configuration import BuilderConfig
from nsc_builder.constants import RESULT_STATUSES, normalize_observed_status
from nsc_builder.execution import ExecutionEngine
from nsc_builder.history import history_record
from nsc_builder.inventory import VISIBLE_STATE_GROUPS
from nsc_builder.models import TestCase, TestVariant


FORMER_KPD_IDS = (
    "TL-LANG-019",
    "TL-LANG-035",
    "TL-LANG-099",
    "TL-LANG-121",
    "TL-LANG-230",
    "TL-LANG-250",
    "TL-LANG-262",
    "TA-AST-014",
    "TA-AST-016",
    "TA-AST-215",
    "TL-LEX-075",
    "TP-PAR-063",
    "TP-PAR-069",
    "TP-PAR-103",
    "TP-PAR-306",
    "TP-PAR-309",
    "TP-PAR-316",
    "TP-PAR-327",
    "TP-PAR-334",
    "TP-PAR-355",
    "TP-PAR-356",
    "TP-PAR-358",
    "TP-PAR-400",
    "TP-PAR-405",
    "TP-PAR-406",
    "TP-PAR-422",
    "TP-PAR-431",
    "TP-PAR-450",
    "TP-PAR-476",
    "TP-PAR-511",
    "TP-PAR-514",
    "TP-PAR-524",
    "TS-SEMA-106",
    "TS-SEMA-110",
    "TS-SEMA-246",
    "TS-SEMA-265",
    "TS-SEMA-317",
    "TF-CFG-076",
    "TF-DOM-142",
    "TF-LANGUAGE-SERVICE-304",
    "TSH-00213",
    "TSH-00214",
    "TSH-00442",
    "TSH-00903",
    "TSH-01011",
    "TSH-01023",
    "T12B-00021",
    "T12B-00022",
    "T12B-00457",
    "T12B-00458",
    "T12B-00460",
    "T12B-00461",
)


def make_case(
    case_id: str,
    *,
    disposition: str = "KnownProductDefect",
    expected_outcome: str = "KNOWN_FAILURE",
) -> TestCase:
    return TestCase(
        case_id=case_id,
        contract_id="CI-OBSERVED-001",
        component="TestInfrastructure",
        wave=1,
        purpose="separate observed results from expectation metadata",
        layer="component",
        profile="PR",
        build_modes=("Debug",),
        optimizations=("NA",),
        executor="nsc_component_tests",
        disposition=disposition,
        objective="publish the observed oracle result",
        expected_outcome=expected_outcome,
        manifest={},
    )


class ObservedResultTests(unittest.TestCase):
    def test_all_52_stale_expected_failures_project_satisfied_oracles_as_pass(self) -> None:
        self.assertEqual(len(FORMER_KPD_IDS), 52)
        self.assertEqual(len(set(FORMER_KPD_IDS)), 52)
        engine = ExecutionEngine(
            SimpleNamespace(),
            SimpleNamespace(root=Path.cwd()),
            lambda _line: None,
            lambda *_args: None,
            lambda *_args: None,
        )

        statuses = {
            case_id: engine._expected_status(
                make_case(case_id), True, "oracle satisfied", "evidence.json"
            )[0]
            for case_id in FORMER_KPD_IDS
        }

        self.assertEqual(set(statuses.values()), {"PASS"})

    def test_failed_oracle_stays_fail_even_with_known_defect_metadata(self) -> None:
        engine = ExecutionEngine(
            SimpleNamespace(),
            SimpleNamespace(root=Path.cwd()),
            lambda _line: None,
            lambda *_args: None,
            lambda *_args: None,
        )

        status, detail, evidence = engine._expected_status(
            make_case("TS-SEMA-265"), False, "observed failure", "failure.json"
        )

        self.assertEqual((status, detail, evidence), ("FAIL", "observed failure", "failure.json"))

    def test_legacy_expectation_states_are_read_only_normalizations(self) -> None:
        visible = set().union(*VISIBLE_STATE_GROUPS.values())
        for legacy, observed in (("KNOWN_FAILURE", "FAIL"), ("UNEXPECTED_PASS", "PASS")):
            with self.subTest(legacy=legacy):
                self.assertNotIn(legacy, RESULT_STATUSES)
                self.assertNotIn(legacy, visible)
                self.assertEqual(normalize_observed_status(legacy), observed)

    def test_legacy_history_remains_readable_without_republishing_legacy_states(self) -> None:
        with tempfile.TemporaryDirectory(prefix="builder-history-", dir=Path.cwd()) as directory:
            root = Path(directory) / "legacy-run"
            root.mkdir()
            (root / "metadata.json").write_text(
                json.dumps(
                    {
                        "kind": "phase-2-suite",
                        "run_id": "legacy-run",
                        "variant_count": 1,
                    }
                ),
                encoding="utf-8",
            )
            counts = {"KNOWN_FAILURE": 1}
            (root / "results.json").write_text(
                json.dumps(
                    {
                        "run_id": "legacy-run",
                        "finished_at": "2026-10-12T00:00:00Z",
                        "status": "COMPLETED_WITH_KNOWN_DEFECTS",
                        "counts": counts,
                        "results": [{"key": "TP-PAR-524|Debug|NA", "status": "KNOWN_FAILURE"}],
                    }
                ),
                encoding="utf-8",
            )
            (root / "summary.md").write_text("legacy evidence\n", encoding="utf-8")
            (root / "events.jsonl").write_text(
                json.dumps(
                    {
                        "kind": "RUN_FINISHED",
                        "payload": {
                            "status": "COMPLETED_WITH_KNOWN_DEFECTS",
                            "counts": counts,
                        },
                    }
                )
                + "\n",
                encoding="utf-8",
            )

            record = history_record(root)

        self.assertEqual(record[2], "COMPLETED_WITH_KNOWN_DEFECTS")

    def test_run_variants_are_rebound_to_fresh_catalogue_metadata(self) -> None:
        stale = make_case("TP-PAR-524")
        fresh = make_case(
            "TP-PAR-524", disposition="Executable", expected_outcome="PASS"
        )
        variant = TestVariant(stale, "Debug", "NA")

        refreshed = refresh_run_variants(CanonicalCatalog((fresh,)), (variant,))

        self.assertEqual(len(refreshed), 1)
        self.assertIs(refreshed[0].case, fresh)
        self.assertEqual(refreshed[0].case.expected_outcome, "PASS")


class CanonicalIdentityInputTests(unittest.TestCase):
    def test_tb_bas_identity_inputs_use_canonical_roots_and_configure_with_defines(self) -> None:
        with tempfile.TemporaryDirectory(prefix="builder-identity-", dir=Path.cwd()) as directory:
            root = Path(directory)
            selected = root / "selected" / "bin" / "nsc.exe"
            comparison = root / "install" / "bin" / "nsc.exe"
            header = (
                root
                / "compiler"
                / "generated"
                / "nativesharp"
                / "Basic"
                / "BuildId.h"
            )
            for path in (selected, comparison, header):
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b"fixture")
            config = BuilderConfig(
                nsc_executable=str(selected),
                nslink_executable=str(root / "selected" / "bin" / "nslink.exe"),
                compiler_build_root=str(root / "compiler"),
                install_prefix=str(root / "install"),
                build_root=str(root / "tests"),
                compiler_llvm_root=str(root / "llvm"),
            ).with_canonical_identity_inputs()

            self.assertEqual(Path(config.comparison_nsc), comparison.resolve())
            self.assertEqual(Path(config.build_id_header), header.resolve())

            tools = {
                "cmake": str(root / "llvm" / "bin" / "cmake.exe"),
                "ninja": str(root / "llvm" / "bin" / "ninja.exe"),
                "clang_cl": str(root / "llvm" / "bin" / "clang-cl.exe"),
                "lld_link": str(root / "llvm" / "bin" / "lld-link.exe"),
                "llvm_mt": str(root / "llvm" / "bin" / "llvm-mt.exe"),
                "llvm_rc": str(root / "llvm" / "bin" / "llvm-rc.exe"),
                "python": sys.executable,
                "llvm_dir": str(root / "llvm" / "lib" / "cmake" / "llvm"),
            }
            with patch("nsc_builder.build.discover_build_tools", return_value=tools):
                command = configure_commands(config, ("Debug",))[0][1]

            self.assertIn(
                "-DNSC_COMPILER_SUITE_COMPARISON_NSC_EXECUTABLE="
                + comparison.resolve().as_posix(),
                command,
            )
            self.assertIn(
                "-DNSC_COMPILER_SUITE_BUILD_ID_HEADER=" + header.resolve().as_posix(),
                command,
            )
            self.assertNotIn("-UNSC_COMPILER_SUITE_COMPARISON_NSC_EXECUTABLE", command)
            self.assertNotIn("-UNSC_COMPILER_SUITE_BUILD_ID_HEADER", command)


if __name__ == "__main__":
    unittest.main()
