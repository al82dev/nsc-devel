from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[3]
HARNESS_ROOT = REPO_ROOT / "tests" / "harness"
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))
if str(HARNESS_ROOT) not in sys.path:
    sys.path.insert(0, str(HARNESS_ROOT))

from _nscbuilder2.model import (  # noqa: E402
    BuilderConfig,
    RunPlan,
    RunSelection,
    RunStep,
    create_run_plan,
)
from _nscbuilder2.compiler import (  # noqa: E402
    CompilerBuildOptions,
    CompilerReceipt,
    create_compiler_build_plan,
    create_compiler_receipt,
    create_test_run_plan,
    validate_compiler_receipt,
)
from _nscbuilder2.pipeline import PipelineRunner  # noqa: E402
from _nscbuilder2.storage import BuilderStorage  # noqa: E402
from _nscbuilder2.summary import condensed_errors  # noqa: E402
from _nscbuilder2.toolchain import (  # noqa: E402
    ToolBuildOptions,
    ToolReceipt,
    create_tool_build_plan,
    create_tool_receipt,
    stage_tool_receipts,
    validate_tool_receipt,
)
from nsc_test.workspace import remove_tree_iterative  # noqa: E402


class NscBuilder2Tests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = Path(tempfile.mkdtemp(dir=REPO_ROOT, prefix="builder2-test-"))
        self.nslink = self.temporary / "nslink.exe"
        self.nslink.write_bytes(b"nslink")
        self.prefix = self.temporary / "prefix"
        (self.prefix / "bin").mkdir(parents=True)
        (self.prefix / "lib").mkdir()
        self.config = BuilderConfig(
            REPO_ROOT,
            self.temporary / "build",
            self.nslink,
            self.prefix,
            4,
        )

    def tearDown(self) -> None:
        remove_tree_iterative(self.temporary)

    def test_plan_builds_once_and_uses_only_v2_ctest_entries(self) -> None:
        history = self.temporary / "history" / "run"
        work = self.temporary / "work" / "run"
        history.mkdir(parents=True)
        work.mkdir(parents=True)
        plan = create_run_plan(
            self.config,
            RunSelection(True, "all", "", True),
            "run",
            history,
            work,
        )
        self.assertEqual([step.identifier for step in plan.steps], ["configure", "build", "unit", "manifests", "specialized"])
        commands = "\n".join(" ".join(step.command) for step in plan.steps)
        self.assertIn("NSC_TEST_INFRASTRUCTURE_V2=ON", commands)
        self.assertIn("^nsc\\.unit\\.consolidated$", commands)
        self.assertIn("^nsc\\.harness\\.cases-o0$", commands)
        self.assertIn("^nsc\\.harness\\.specialized-o0$", commands)
        self.assertNotIn("C:\\native-sharp", commands)
        self.assertNotIn("_nscbuilder\\", commands)

    def test_direct_case_freezes_built_nsc_and_explicit_nslink(self) -> None:
        history = self.temporary / "history" / "run"
        work = self.temporary / "work" / "run"
        history.mkdir(parents=True)
        work.mkdir(parents=True)
        plan = create_run_plan(
            self.config,
            RunSelection(False, "case", "nsc.codegen-target", False),
            "run",
            history,
            work,
        )
        command = plan.steps[-1].command
        self.assertEqual(command[0], "python")
        self.assertIn(str(plan.compiler), command)
        self.assertIn(str(self.nslink.resolve()), command)
        self.assertIn("nsc.codegen-target", command)

    def test_configuration_and_history_survive_new_storage_instance(self) -> None:
        storage_root = self.temporary / "storage"
        storage = BuilderStorage(storage_root)
        storage.save_config(self.config)
        run_id, history, _work = storage.new_run()
        BuilderStorage.write_run(history, {"run_id": run_id, "status": "passed"})
        reopened = BuilderStorage(storage_root)
        self.assertEqual(reopened.load_config(self.config).to_dict(), self.config.to_dict())
        self.assertEqual(reopened.load_history()[0]["run_id"], run_id)

    def test_condensed_report_deduplicates_signals(self) -> None:
        errors = condensed_errors(
            "error: broken\n"
            "ERROR:   broken\n"
            "noise\n"
            "failed=0\n"
            "0 failed, 6 passed\n"
            "100% tests passed\n"
            "CMake Error at x:1\n"
        )
        self.assertEqual(errors, ["error: broken", "CMake Error at x:1"])

    def test_run_record_is_valid_json(self) -> None:
        history = self.temporary / "history"
        history.mkdir()
        BuilderStorage.write_run(history, {"run_id": "x", "status": "running"})
        self.assertEqual(json.loads((history / "run.json").read_text(encoding="utf-8"))["status"], "running")

    def test_tool_plans_publish_complete_release_and_debug_products(self) -> None:
        expected = {
            "nslink": ("nslink.exe",),
            "nsasm": ("nsasm.exe", "nsdump.exe", "nsdis.exe"),
            "nsdbg": ("nsdbg.exe",),
        }
        for name, products in expected.items():
            source = self.temporary / name
            source.mkdir()
            (source / "CMakeLists.txt").write_text("# fixture\n", encoding="utf-8")
            configuration = "Debug" if name == "nsdbg" else "Release"
            plan = create_tool_build_plan(
                ToolBuildOptions(
                    name,
                    source,
                    source / f"build-{configuration.casefold()}",
                    source / "bin" / configuration.casefold(),
                    configuration,
                    7,
                ),
                f"{name}-run",
                self.temporary / f"{name}-history",
                self.temporary / f"{name}-work",
            )
            self.assertEqual(tuple(path.name for path in plan.products), products)
            commands = "\n".join(" ".join(step.command) for step in plan.steps)
            self.assertIn("cmake --build", commands)
            self.assertIn(f"CMAKE_BUILD_TYPE={configuration}", commands)
            self.assertNotIn("C:\\", commands)

    def test_compiler_build_is_separate_and_emits_reusable_receipt(self) -> None:
        options = CompilerBuildOptions(
            REPO_ROOT,
            self.temporary / "compiler-build",
            self.nslink,
            self.prefix,
            "Debug",
            "STATIC",
            "O0",
            "ninja",
            True,
            6,
        )
        plan = create_compiler_build_plan(
            options,
            "compiler-run",
            self.temporary / "compiler-history",
            self.temporary / "compiler-work",
        )
        self.assertEqual([step.identifier for step in plan.steps], ["compiler-configure", "compiler-build"])
        commands = "\n".join(" ".join(step.command) for step in plan.steps)
        self.assertIn("--fresh", commands)
        self.assertIn("ninja -C", commands)
        self.assertNotIn("ctest", commands)
        self.assertNotIn("pruebas", commands)
        plan.compiler.parent.mkdir(parents=True)
        plan.compiler.write_bytes(b"nsc-fixture")
        receipt = create_compiler_receipt(plan, "passed")
        restored = CompilerReceipt.from_dict(receipt.to_dict())
        self.assertEqual(validate_compiler_receipt(restored), plan.compiler.resolve())

        test_plan = create_test_run_plan(
            restored,
            RunSelection(False, "all", "", False, ("O0", "O1", "O2", "O3")),
            "test-run",
            self.temporary / "test-history",
            self.temporary / "test-work",
        )
        self.assertEqual(
            [step.identifier for step in test_plan.steps],
            ["manifests-o0", "manifests-o1", "manifests-o2", "manifests-o3"],
        )
        test_commands = "\n".join(" ".join(step.command) for step in test_plan.steps)
        self.assertNotIn("compiler-configure", test_commands)
        self.assertNotIn("--target nsc ", test_commands)
        for optimization in ("O0", "O1", "O2", "O3"):
            self.assertIn(f"--optimization {optimization}", test_commands)

    def test_tool_receipt_requires_all_products_and_detects_mutation(self) -> None:
        source = self.temporary / "nsasm"
        source.mkdir()
        (source / "CMakeLists.txt").write_text("# fixture\n", encoding="utf-8")
        output = source / "bin" / "release"
        output.mkdir(parents=True)
        plan = create_tool_build_plan(
            ToolBuildOptions("nsasm", source, source / "build", output),
            "nsasm-run",
            self.temporary / "receipt-history",
            self.temporary / "receipt-work",
        )
        for product in plan.products:
            product.write_bytes(product.name.encode("ascii"))
        receipt = create_tool_receipt(plan, "passed")
        self.assertEqual(len(validate_tool_receipt(receipt)), 3)
        plan.products[-1].write_bytes(b"changed")
        with self.assertRaisesRegex(ValueError, "changed"):
            validate_tool_receipt(receipt)

    def test_staging_copies_only_receipted_products_and_writes_manifest(self) -> None:
        source = self.temporary / "nslink-source"
        source.mkdir()
        (source / "CMakeLists.txt").write_text("# fixture\n", encoding="utf-8")
        output = source / "bin" / "release"
        output.mkdir(parents=True)
        plan = create_tool_build_plan(
            ToolBuildOptions("nslink", source, source / "build", output),
            "nslink-run",
            self.temporary / "stage-history",
            self.temporary / "stage-work",
        )
        plan.products[0].write_bytes(b"verified-nslink")
        receipt = create_tool_receipt(plan, "passed")
        restored = ToolReceipt.from_dict(receipt.to_dict())
        destination = self.temporary / "compiler" / "bin"
        manifest_path = self.temporary / "stage" / "toolchain-manifest.json"
        manifest = stage_tool_receipts((restored,), destination, manifest_path)
        self.assertEqual((destination / "nslink.exe").read_bytes(), b"verified-nslink")
        self.assertEqual(len(manifest["artifacts"]), 1)
        self.assertTrue(manifest_path.is_file())

    @unittest.skipUnless(sys.platform == "win32", "Windows process-tree contract")
    def test_cancel_terminates_only_runner_descendants(self) -> None:
        history = self.temporary / "cancel-history"
        work = self.temporary / "cancel-work"
        history.mkdir()
        work.mkdir()
        child_pid_file = work / "child.pid"
        parent_script = work / "parent.py"
        parent_script.write_text(
            "import subprocess,sys,time\n"
            "from pathlib import Path\n"
            "child=subprocess.Popen([sys.executable,'-c','import time; time.sleep(30)'])\n"
            "Path(sys.argv[1]).write_text(str(child.pid),encoding='utf-8')\n"
            "time.sleep(30)\n",
            encoding="utf-8",
        )
        plan = RunPlan(
            "cancel",
            self.config,
            RunSelection(True, "none", "", False),
            history,
            work,
            self.temporary / "unused-nsc.exe",
            (
                RunStep(
                    "cancel",
                    "Cancellation fixture",
                    (sys.executable, str(parent_script), str(child_pid_file)),
                    REPO_ROOT,
                ),
            ),
        )
        sentinel = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(30)"])
        runner = PipelineRunner()
        result_box = []
        thread = threading.Thread(target=lambda: result_box.append(runner.run(plan)))
        try:
            thread.start()
            deadline = time.monotonic() + 10
            while not child_pid_file.is_file() and time.monotonic() < deadline:
                time.sleep(0.05)
            self.assertTrue(child_pid_file.is_file())
            child_pid = int(child_pid_file.read_text(encoding="utf-8"))
            runner.cancel()
            thread.join(timeout=10)
            self.assertFalse(thread.is_alive())
            self.assertEqual(result_box[0].status, "cancelled")
            self.assertIsNone(sentinel.poll(), "unrelated process was terminated")
            listing = subprocess.run(
                ["tasklist", "/FI", f"PID eq {child_pid}", "/FO", "CSV", "/NH"],
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                encoding="utf-8",
                errors="replace",
                check=False,
            ).stdout
            self.assertNotIn(f'"{child_pid}"', listing)
        finally:
            runner.cancel()
            sentinel.kill()
            sentinel.wait(timeout=5)
            if thread.is_alive():
                thread.join(timeout=5)


if __name__ == "__main__":
    unittest.main()
