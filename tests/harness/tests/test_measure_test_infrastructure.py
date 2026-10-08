from __future__ import annotations

import sys
import tempfile
import unittest
import json
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[3]
TOOLS_ROOT = REPO_ROOT / "tools"
if str(TOOLS_ROOT) not in sys.path:
    sys.path.insert(0, str(TOOLS_ROOT))

from measure_test_infrastructure import (  # noqa: E402
    directory_size_iterative,
    harness_summary,
    logical_executable_names,
    ninja_log_metrics,
    parse_ctest_count,
)
from nsc_test.workspace import remove_tree_iterative  # noqa: E402


class TestInfrastructureMetricsTests(unittest.TestCase):
    def test_parses_ctest_total(self) -> None:
        self.assertEqual(parse_ctest_count("Test project A:/x\nTotal Tests: 684\n"), 684)

    def test_counts_logical_executables_without_alias_duplicates(self) -> None:
        output = "\n".join(
            [
                "bin/nsc.exe: CXX_EXECUTABLE_LINKER__nsc_Release",
                "nsc.exe: phony",
                "tests/nsc_unit_tests.exe: CXX_EXECUTABLE_LINKER__tests",
                "nsc_unit_tests.exe: phony",
                "tests/object.obj: CXX_COMPILER__tests",
            ]
        )
        self.assertEqual(logical_executable_names(output), {"nsc.exe", "nsc_unit_tests.exe"})

    def test_directory_size_is_iterative_and_ignores_symlinks(self) -> None:
        temporary = Path(tempfile.mkdtemp(dir=REPO_ROOT, prefix="metrics-test-"))
        try:
            (temporary / "one.bin").write_bytes(b"123")
            nested = temporary / "nested"
            nested.mkdir()
            (nested / "two.bin").write_bytes(b"4567")
            self.assertEqual(directory_size_iterative(temporary), (2, 7))
        finally:
            remove_tree_iterative(temporary)

    def test_harness_summary_honors_verified_negative_cases(self) -> None:
        temporary = Path(tempfile.mkdtemp(dir=REPO_ROOT, prefix="metrics-summary-"))
        try:
            summary = temporary / "summary.json"
            summary.write_text(
                json.dumps(
                    {
                        "results": [
                            {
                                "case_id": "expected-error",
                                "exit_code": 1,
                                "passed": True,
                                "duration_seconds": 0.25,
                            },
                            {
                                "case_id": "broken",
                                "exit_code": 0,
                                "passed": False,
                                "duration_seconds": 0.5,
                            },
                            {
                                "case_id": "specialized-owner",
                                "exit_code": 0,
                                "duration_seconds": 0.75,
                            },
                        ]
                    }
                ),
                encoding="utf-8",
            )
            observed = harness_summary(summary)
            self.assertEqual(observed["selected"], 3)
            self.assertEqual(observed["passed"], 2)
            self.assertEqual(observed["failed"], 1)
            self.assertEqual(observed["summed_case_seconds"], 1.5)
        finally:
            remove_tree_iterative(temporary)

    def test_summarizes_ninja_compile_and_link_edges(self) -> None:
        temporary = Path(tempfile.mkdtemp(dir=REPO_ROOT, prefix="metrics-ninja-"))
        try:
            log = temporary / ".ninja_log"
            log.write_text(
                "# ninja log v5\n"
                "0\t100\t1\ta.obj\th1\n"
                "10\t160\t2\tb.obj\th2\n"
                "160\t220\t3\tapp.exe\th3\n",
                encoding="utf-8",
            )
            observed = ninja_log_metrics(log)
            self.assertEqual(observed["recorded_outputs"], 3)
            self.assertEqual(observed["build_span_milliseconds"], 220)
            self.assertEqual(observed["compile_objects"]["edges"], 2)
            self.assertEqual(observed["compile_objects"]["aggregate_milliseconds"], 250)
            self.assertEqual(observed["link_executables"]["aggregate_milliseconds"], 60)
        finally:
            remove_tree_iterative(temporary)


if __name__ == "__main__":
    unittest.main()
