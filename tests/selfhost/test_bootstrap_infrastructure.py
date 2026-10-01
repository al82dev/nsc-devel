from __future__ import annotations

import hashlib
import json
import os
import runpy
import shutil
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

import nscbuilder
from selfhost.bootstrap import (
    clean_selfhost,
    verify_environment,
    verify_llvm_c_contract,
    verify_nsc_next_skeleton,
    verify_frontend,
    verify_semantics,
    verify_ir,
)


TEST_TEMP_ROOT = ROOT / "_nscbuilder" / "temp" / "selfhost-unit-tests"


class BootstrapInfrastructureTests(unittest.TestCase):
    def setUp(self) -> None:
        TEST_TEMP_ROOT.mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(dir=TEST_TEMP_ROOT)
        self.root = Path(self.temporary.name)

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def test_import_uses_headless_signals_without_loading_qt(self) -> None:
        self.assertTrue(nscbuilder.HEADLESS)
        worker = nscbuilder.PipelineWorker(nscbuilder.make_baseline_plan(1), [])
        received: list[str] = []
        worker.stage_started.connect(received.append)
        worker.stage_started.emit("probe")
        self.assertEqual(received, ["probe"])

    def test_configuration_precedence_environment_over_file(self) -> None:
        config = {"paths": {"prefix": "A:\\from-config"}}
        with patch.dict(os.environ, {"NSC_INSTALL_ROOT": "A:\\from-environment"}, clear=False):
            self.assertEqual(
                nscbuilder.configured_path(config, "prefix", "A:\\default"),
                "A:\\from-environment",
            )

    def test_configuration_uses_file_before_default(self) -> None:
        config = {"paths": {"prefix": "A:\\from-config"}}
        with patch.dict(os.environ, {}, clear=True):
            self.assertEqual(
                nscbuilder.configured_path(config, "prefix", "A:\\default"),
                "A:\\from-config",
            )

    def test_reproducible_build_id_uses_environment_before_file(self) -> None:
        config = {"build_id_override": "2000000000000000"}
        with patch.dict(
            os.environ,
            {"NSC_BUILD_ID_OVERRIDE": "2000000000000001"},
            clear=False,
        ):
            self.assertEqual(nscbuilder.configured_build_id(config), "2000000000000001")

    def test_reproducible_build_id_rejects_invalid_value(self) -> None:
        with patch.dict(os.environ, {}, clear=True):
            with self.assertRaises(ValueError):
                nscbuilder.configured_build_id({"build_id_override": "not-a-build-id"})

    def test_source_manifest_includes_selfhost_but_not_generated_state(self) -> None:
        (self.root / "selfhost" / "bootstrap").mkdir(parents=True)
        (self.root / "selfhost" / "bootstrap" / "bootstrap.lock").write_text("lock")
        (self.root / "selfhost" / "out").mkdir()
        (self.root / "selfhost" / "out" / "generated.txt").write_text("generated")
        (self.root / "selfhost" / "evidence" / "runs").mkdir(parents=True)
        (self.root / "selfhost" / "evidence" / "runs" / "report.md").write_text("report")
        (self.root / "selfhost" / "evidence" / "gates").mkdir()
        (self.root / "selfhost" / "evidence" / "gates" / "gate.json").write_text("gate")
        relative = {
            path.relative_to(self.root).as_posix()
            for path in nscbuilder.collect_source_files(self.root)
        }
        self.assertIn("selfhost/bootstrap/bootstrap.lock", relative)
        self.assertNotIn("selfhost/out/generated.txt", relative)
        self.assertNotIn("selfhost/evidence/runs/report.md", relative)
        self.assertNotIn("selfhost/evidence/gates/gate.json", relative)

    def test_library_source_marker_matches_tracked_file(self) -> None:
        self.assertEqual((ROOT / "lib" / ".source.txt").read_bytes(), nscbuilder.LIBRARY_SOURCE_MARKER)

    def test_write_guard_rejects_destination_outside_allowed_root(self) -> None:
        allowed = self.root / "allowed"
        plan = nscbuilder.make_baseline_plan(
            1,
            compiler_build_dir=allowed / "build",
            prefix=self.root / "forbidden",
            test_build_lld=allowed / "test-lld",
            test_build_nslink=allowed / "test-nslink",
            history_root=allowed / "history",
            test_work_root=allowed / "work",
            temp_root=allowed / "temp",
            allowed_write_roots=[allowed],
        )
        rejected = nscbuilder.validate_plan_write_paths(plan)
        self.assertIn(str((self.root / "forbidden").resolve()), rejected)
        self.assertNotIn(str((allowed / "build").resolve()), rejected)

    def test_baseline_plan_defaults_to_nslink_with_selective_lld_diagnostics(self) -> None:
        plan = nscbuilder.make_baseline_plan(4)
        self.assertEqual(Path(plan.prefix), Path(r"A:\native-sharp-3.0-baseline"))
        self.assertEqual(Path(plan.deploy_prefix), Path(r"C:\native-sharp"))
        self.assertEqual(
            Path(plan.nslink_exe),
            Path(r"A:\nslink-devel\build-debug\bin\nslink.exe"),
        )
        self.assertFalse(plan.sync_libraries)
        self.assertEqual(plan.optimizations, ["-O0", "-O1", "-O2", "-O3"])
        self.assertEqual(plan.linkers, ["NSLink"])
        self.assertTrue(plan.lld_diagnostics)
        self.assertTrue(plan.run_ctest)
        self.assertTrue(plan.run_cases)
        self.assertTrue(plan.run_triage)
        self.assertTrue(plan.certify_baseline)
        self.assertTrue(plan.reuse_results)

    def test_debug_ctests_inherit_the_linker_selected_by_nscbuilder(self) -> None:
        cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        for test_name in (
            "nsc.debug.codeview-pdb",
            "nsc.debug.extreme-o0",
            "nsc.debug.inline-stepping-o0",
            "nsc.debug.assembly-stepping-o0",
        ):
            self.assertIn(f"NAME {test_name}", cmake)
        self.assertNotIn(
            '"NSC_LLD_LINK=${NSC_DEBUG_LLD_LINK_EXECUTABLE}"',
            cmake,
        )

    def test_deep_baseline_plan_runs_both_linkers_without_a_third_diagnostic_pass(self) -> None:
        plan = nscbuilder.make_baseline_plan(
            4,
            linkers=["NSLink", "lld-link"],
        )
        self.assertEqual(plan.linkers, ["NSLink", "lld-link"])
        self.assertFalse(plan.lld_diagnostics)
        self.assertTrue(plan.certify_baseline)

    def test_reduced_baseline_plan_cannot_certify(self) -> None:
        plan = nscbuilder.make_baseline_plan(
            4,
            linkers=["lld-link"],
            optimizations=["-O0"],
        )
        self.assertEqual(plan.linkers, ["lld-link"])
        self.assertEqual(plan.optimizations, ["-O0"])
        self.assertFalse(plan.certify_baseline)
        self.assertFalse(plan.lld_diagnostics)

    def test_lld_diagnostic_repeats_only_exact_nslink_failures(self) -> None:
        plan = nscbuilder.make_baseline_plan(2)
        worker = nscbuilder.PipelineWorker(plan, [])
        worker.run_dir = self.root / "diagnostic-run"
        worker.run_dir.mkdir()
        worker.metadata = {"id": "diagnostic", "stages": [], "cases": [], "failures": []}
        worker.total_units = 10
        worker.failed = 3
        fixture = nscbuilder.CaseSpec("tests/probe.nsproj", "fixture-probe", 42, "fixture")
        triage = nscbuilder.CaseSpec("tests/triage.nsproj", "triage-probe", 42, "bug triage")
        with (
            patch.object(worker, "_ctest", return_value=[]) as ctest,
            patch.object(worker, "_run_case_group", return_value=[]) as cases,
            patch.object(worker, "_triage", return_value=[]) as triage_run,
        ):
            worker._diagnose_nslink_failures(
                ["nsc.failed.test"],
                [(fixture, "-O0")],
                [(triage, "-O2")],
            )
        ctest.assert_called_once_with(
            "lld-link",
            selected_ctests=["nsc.failed.test"],
            build_dir=Path(plan.test_build_dirs["NSLink"]),
            diagnostic=True,
        )
        cases.assert_called_once_with(
            "lld-link", diagnostic_work=[(fixture, "-O0")]
        )
        triage_run.assert_called_once_with(
            "lld-link", diagnostic_work=[(triage, "-O2")]
        )
        self.assertEqual(worker.failed, 3)
        self.assertEqual(worker.total_units, 13)
        self.assertEqual(
            {row["result"] for row in worker.metadata["lld_diagnostics"]},
            {"nslink-only"},
        )

    def test_failed_lld_diagnostic_never_counts_as_a_new_primary_failure(self) -> None:
        plan = nscbuilder.make_baseline_plan(1, reuse_results=False)
        worker = nscbuilder.PipelineWorker(plan, [])
        worker.run_dir = self.root / "ctest-diagnostic"
        worker.run_dir.mkdir()
        worker.metadata = {"id": "diagnostic", "stages": [], "cases": [], "failures": []}
        build = self.root / "shared-ctest"
        build.mkdir()
        (build / "CTestTestfile.cmake").write_text("# configured\n")
        output = "1/1 Test #1: nsc.failed.test ***Failed 0.01 sec\n"
        with (
            patch.object(
                worker,
                "_ctest_result_cache",
                return_value=(self.root / "no-cache.json", "diagnostic-id"),
            ),
            patch.object(
                worker,
                "_run_process",
                return_value=nscbuilder.ProcessResult(8, 0.01, output),
            ),
        ):
            failed = worker._ctest(
                "lld-link",
                selected_ctests=["nsc.failed.test"],
                build_dir=build,
                diagnostic=True,
            )
        self.assertEqual(failed, ["nsc.failed.test"])
        self.assertEqual(worker.failed, 0)
        self.assertEqual(worker.stage_rows[-1]["status"], "failed")
        self.assertEqual(worker.failure_rows, [])

    def test_scoped_baseline_never_writes_gate_or_promotes(self) -> None:
        plan = nscbuilder.make_baseline_plan(
            1,
            history_root=self.root / "history",
            linkers=["lld-link"],
            optimizations=["-O0"],
        )
        worker = nscbuilder.PipelineWorker(plan, [])
        worker.run_dir = self.root / "run"
        worker.run_dir.mkdir()
        worker.metadata = {"id": "scoped-test", "stages": []}
        after = {"source_digest": "same", "binaries": {}, "tools": {}}
        with (
            patch.object(worker, "_abi_measure", return_value=after),
            patch.object(worker, "_abi_compare", return_value=[]),
            patch.object(worker, "_abi_gate_state") as gate_state,
            patch("nscbuilder.promote_certified_baseline") as promote,
        ):
            worker._abi_baseline({"source_digest": "same"}, after)
        gate_state.assert_not_called()
        promote.assert_not_called()
        scoped = json.loads((worker.run_dir / "scoped-validation.json").read_text())
        self.assertEqual(scoped["state"], "SCOPED-PASSED")
        self.assertFalse(scoped["certified"])

    def test_incremental_identity_uses_content_instead_of_timestamps(self) -> None:
        project = self.root / "Probe.nsproj"
        source = self.root / "Main.ns"
        compiler = self.root / "nsc.exe"
        linker = self.root / "lld-link.exe"
        project.write_text('<Project><Compile Include="Main.ns" /></Project>')
        source.write_text("namespace Probe;\n")
        compiler.write_bytes(b"compiler")
        linker.write_bytes(b"linker")
        before = nscbuilder.incremental_fingerprint(project, compiler, linker)
        os.utime(compiler, None)
        self.assertEqual(before, nscbuilder.incremental_fingerprint(project, compiler, linker))
        source.write_text("namespace Changed;\n")
        self.assertNotEqual(before, nscbuilder.incremental_fingerprint(project, compiler, linker))

    def test_execution_identity_covers_adjacent_runtime_dlls(self) -> None:
        executable = self.root / "probe.exe"
        runtime = self.root / "probe.runtime.dll"
        executable.write_bytes(b"exe")
        runtime.write_bytes(b"runtime-a")
        before = nscbuilder.executable_execution_fingerprint(executable, 42)
        runtime.write_bytes(b"runtime-b")
        self.assertNotEqual(
            before,
            nscbuilder.executable_execution_fingerprint(executable, 42),
        )

    def test_ctest_approved_result_is_reused_without_spawning_ctest(self) -> None:
        plan = nscbuilder.make_baseline_plan(1, reuse_results=True)
        worker = nscbuilder.PipelineWorker(plan, [])
        worker.run_dir = self.root / "run"
        worker.run_dir.mkdir()
        worker.metadata = {"id": "current", "stages": []}
        worker.ctest_ready_linkers.add("lld-link")
        cache = self.root / "ctest-cache.json"
        nscbuilder.write_json(
            cache,
            {"identity": "exact", "status": "passed", "run_id": "prior"},
        )
        with (
            patch.object(worker, "_ctest_result_cache", return_value=(cache, "exact")),
            patch.object(worker, "_run_process", side_effect=AssertionError("must not run")),
        ):
            worker._ctest("lld-link")
        self.assertEqual(worker.stage_rows[-1]["status"], "reused")

    def test_baseline_staging_is_reset_only_at_the_fixed_a_drive_path(self) -> None:
        staging = self.root / "native-sharp-3.0-baseline"
        staging.mkdir()
        (staging / "stale.txt").write_text("stale")
        with patch.object(nscbuilder, "DEFAULT_BASELINE_PREFIX", staging):
            nscbuilder.reset_baseline_prefix(staging)
            self.assertTrue(staging.is_dir())
            self.assertFalse((staging / "stale.txt").exists())
            with self.assertRaises(ValueError):
                nscbuilder.reset_baseline_prefix(self.root / "another-directory")

    def test_baseline_stages_nslink_from_a_and_uses_the_staged_copy(self) -> None:
        source = self.root / "nslink-build" / "bin" / "nslink.exe"
        source.parent.mkdir(parents=True)
        source.write_bytes(b"NSLink built on A drive")
        staging = self.root / "native-sharp-3.0-baseline"
        staging.mkdir()
        plan = nscbuilder.make_baseline_plan(1, prefix=staging, nslink_exe=source)
        worker = nscbuilder.PipelineWorker(plan, [])
        with patch.object(nscbuilder, "DEFAULT_BASELINE_PREFIX", staging):
            destination = nscbuilder.stage_baseline_nslink(source, staging)
        self.assertEqual(destination, staging / "bin" / "nslink.exe")
        self.assertEqual(destination.read_bytes(), source.read_bytes())
        self.assertEqual(worker._effective_nslink_executable(), destination.resolve())
        self.assertEqual(Path(worker._base_environment("NSLink")["NSC_LLD_LINK"]), destination)

    def test_builder_disables_only_redundant_automatic_error_logs(self) -> None:
        plan = nscbuilder.make_baseline_plan(1)
        environment = nscbuilder.PipelineWorker(plan, [])._base_environment("lld-link")
        self.assertEqual(environment["NSC_NO_ERROR_LOG"], "1")

        source = (ROOT / "src/nsc/main.cpp").read_text(encoding="utf-8")
        self.assertIn('GetEnvironmentVariableA("NSC_NO_ERROR_LOG"', source)
        self.assertIn('argument == "--no-error-log"', source)

    def test_baseline_rejects_nslink_from_installed_c_drive(self) -> None:
        plan = nscbuilder.make_baseline_plan(
            1,
            prefix=self.root / "native-sharp-3.0-baseline",
            nslink_exe=Path(r"C:\native-sharp\bin\nslink.exe"),
        )
        worker = nscbuilder.PipelineWorker(plan, [])
        problems = worker._validate_paths()
        self.assertTrue(
            any("debe provenir de una construcción en A:" in problem for problem in problems),
            problems,
        )

    def test_certified_promotion_flows_from_staging_to_install_without_deleting_components(self) -> None:
        staging = self.root / "staging"
        destination = self.root / "installed"
        (staging / "bin").mkdir(parents=True)
        (staging / "bin" / "nsc.exe").write_bytes(b"certified nsc")
        (destination / "bin").mkdir(parents=True)
        (destination / "bin" / "nslink.exe").write_bytes(b"external component")
        with (
            patch.object(nscbuilder, "DEFAULT_BASELINE_PREFIX", staging),
            patch.object(nscbuilder, "DEFAULT_PREFIX", destination),
        ):
            result = nscbuilder.promote_certified_baseline(staging, destination, "test-run")
        self.assertEqual((destination / "bin" / "nsc.exe").read_bytes(), b"certified nsc")
        self.assertEqual(
            (destination / "bin" / "nslink.exe").read_bytes(), b"external component"
        )
        self.assertEqual(result["source"], str(staging.resolve()))
        self.assertEqual(result["destination"], str(destination.resolve()))

    def test_baseline_builds_core_package_inside_staging(self) -> None:
        staging = self.root / "staging"
        plan = nscbuilder.make_baseline_plan(3, prefix=staging)
        worker = nscbuilder.PipelineWorker(plan, [])
        with patch.object(worker, "_command_stage", return_value=True) as command_stage:
            self.assertTrue(worker._build_baseline_libraries())
        name, command, environment = command_stage.call_args.args
        self.assertEqual(name, "Construir bibliotecas bootstrap")
        self.assertEqual(Path(command[0]), staging / "bin" / "nsc.exe")
        self.assertIn("--emit-project-oop-lib", command)
        self.assertIn("-O0", command)
        self.assertEqual(Path(command[-1]), staging / "lib" / "NativeSharpCore.lib")
        self.assertEqual(Path(environment["NS_LIB_ROOT"]), staging / "lib")

    def test_driver_uses_ns_lib_root_only_as_a_fallback(self) -> None:
        source = (ROOT / "src" / "Driver" / "Driver.cpp").read_text(encoding="utf-8")
        self.assertIn('_dupenv_s(&environmentRoot, &environmentRootLength, "NS_LIB_ROOT")', source)
        self.assertIn("!libraryReferenceRootSeen", source)
        self.assertIn("!executionScope.projectLoadOptions.libraryReferenceRoot.has_value()", source)

    def test_library_preparation_never_copies_from_the_installed_prefix(self) -> None:
        retired = (
            "all.bat",
            "build_nsc.bat",
            "build-all.bat",
            "check_entorno.bat",
            "nslink_solo.bat",
            "run_bug_triage.bat",
            "run_windows_checklist.bat",
            "test_nsc.bat",
            "tools/build_project.cmd",
            "tools/clean_project.cmd",
            "tools/prepare_libraries.cmd",
            "tools/setup_toolchain.cmd",
            "build_al_dia.py",
            "run_cases.py",
            "report.py",
            "nslink_pipeline.py",
            "verify_manifest.py",
        )
        self.assertEqual([path for path in retired if (ROOT / path).exists()], [])

    def test_cmake_configuration_explicitly_sets_or_clears_build_identity(self) -> None:
        distributable = nscbuilder.make_baseline_plan(1, build_id_override="")
        distributable_command = nscbuilder.PipelineWorker(
            distributable, []
        )._configure_command(build_tests=False)
        self.assertIn("-DNSC_BUILD_ID_OVERRIDE=", distributable_command)

        reproducible = nscbuilder.make_baseline_plan(
            1, build_id_override="2000000000000000"
        )
        reproducible_command = nscbuilder.PipelineWorker(
            reproducible, []
        )._configure_command(build_tests=False)
        self.assertIn(
            "-DNSC_BUILD_ID_OVERRIDE=2000000000000000",
            reproducible_command,
        )

    def test_cmake_configuration_uses_clang_cl_and_static_llvm_components(self) -> None:
        plan = nscbuilder.make_baseline_plan(1)
        command = nscbuilder.PipelineWorker(plan, [])._configure_command(build_tests=False)
        llvm = Path(plan.llvm_root).as_posix()
        self.assertIn(f"-DCMAKE_C_COMPILER={llvm}/bin/clang-cl.exe", command)
        self.assertIn(f"-DCMAKE_CXX_COMPILER={llvm}/bin/clang-cl.exe", command)
        self.assertIn(f"-DCMAKE_LINKER={llvm}/bin/lld-link.exe", command)
        self.assertIn(f"-DLLVM_DIR={llvm}/lib/cmake/llvm", command)
        self.assertIn("-DCMAKE_DISABLE_FIND_PACKAGE_LLVM=FALSE", command)
        self.assertIn("-UNSC_LLVM_MONOLITHIC_LIBRARY", command)
        self.assertNotIn("-DCMAKE_DISABLE_FIND_PACKAGE_LLVM=TRUE", command)
        self.assertFalse(
            any(argument.startswith("-DNSC_LLVM_MONOLITHIC_LIBRARY=") for argument in command)
        )
        self.assertIn("-DNSC_HOST_LINKAGE=STATIC", command)
        self.assertIn("-DNSC_HOST_OPTIMIZATION=O2", command)

    def test_compiler_plan_can_select_dynamic_llvm_and_o0_without_changing_msvc_runtime(self) -> None:
        plan = nscbuilder.make_baseline_plan(1)
        plan.host_linkage = "DYNAMIC"
        plan.host_optimization = "O0"
        command = nscbuilder.PipelineWorker(plan, [])._configure_command(build_tests=False)
        self.assertIn("-DNSC_HOST_LINKAGE=DYNAMIC", command)
        self.assertIn("-DNSC_HOST_OPTIMIZATION=O0", command)

        cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn('set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")', cmake)
        self.assertIn('install(FILES "${NSC_LLVM_C_RUNTIME}" DESTINATION bin)', cmake)
        self.assertIn("NSC_ALLOW_LLVM_C=TRUE", cmake)

    def test_performance_baseline_uses_the_selected_compiler_installation(self) -> None:
        module = runpy.run_path(str(ROOT / "tools/nsc_performance_baseline.py"))
        compiler = self.root / "selected-prefix" / "bin" / "nsc.exe"
        environment = module["make_environment"](
            "lld-link",
            compiler,
            Path(r"C:\clang-llvm"),
            Path(r"C:\native-sharp\bin\nslink.exe"),
        )
        self.assertEqual(
            Path(environment["NS_ROOT"]),
            (self.root / "selected-prefix").resolve(),
        )
        self.assertEqual(
            Path(environment["NS_LIB_ROOT"]),
            (self.root / "selected-prefix" / "lib").resolve(),
        )

    def test_legacy_windows_runners_cover_all_optimization_levels(self) -> None:
        plan = nscbuilder.make_baseline_plan(1)
        self.assertEqual(plan.optimizations, ["-O0", "-O1", "-O2", "-O3"])
        source = (ROOT / "nscbuilder.py").read_text(encoding="utf-8")
        self.assertIn("def _run_case_group", source)
        self.assertIn("def _triage", source)

    def test_configured_build_is_usable_but_not_deletable_until_marked(self) -> None:
        allowed = self.root / "allowed"
        build = allowed / "arbitrary-name"
        self.assertTrue(
            nscbuilder.build_directory_allowed(build, ROOT, "build-test-", [allowed])
        )
        self.assertFalse(nscbuilder.build_directory_can_be_deleted(build, ROOT, "build-test-"))
        nscbuilder.mark_managed_build(build)
        self.assertTrue(nscbuilder.build_directory_can_be_deleted(build, ROOT, "build-test-"))

    def test_environment_verifier_accepts_locked_file(self) -> None:
        artifact = self.root / "artifact.bin"
        artifact.write_bytes(b"NativeSharp bootstrap fixture\n")
        digest = hashlib.sha256(artifact.read_bytes()).hexdigest()
        lock = {
            "schema": "nativesharp-bootstrap-lock",
            "version": 1,
            "target": {"host": "test"},
            "artifacts": [
                {
                    "id": "fixture",
                    "path": "${FIXTURE}",
                    "state": "locked",
                    "sha256": digest,
                }
            ],
        }
        report = verify_environment.verify(lock, {"FIXTURE": str(artifact)})
        self.assertEqual(report["status"], "passed")
        self.assertEqual(report["problems"], [])

    def test_run_bootstrap_lock_captures_selected_artifacts_only(self) -> None:
        nsc0 = self.root / "nsc0.exe"
        nslink = self.root / "nslink.exe"
        llvm = self.root / "LLVM-C.lib"
        nsc0.write_bytes(b"current nsc0")
        nslink.write_bytes(b"current nslink")
        llvm.write_bytes(b"frozen llvm")
        template = self.root / "bootstrap.lock"
        original_llvm_hash = hashlib.sha256(llvm.read_bytes()).hexdigest()
        template.write_text(
            json.dumps(
                {
                    "schema": "nativesharp-bootstrap-lock",
                    "version": 1,
                    "artifacts": [
                        {"id": "nsc0", "sha256": "0" * 64},
                        {"id": "nslink", "sha256": "1" * 64},
                        {"id": "llvm-c-library", "sha256": original_llvm_hash},
                    ],
                }
            ),
            encoding="utf-8",
        )
        output = self.root / "run" / "bootstrap.lock"
        captured = nscbuilder.write_run_bootstrap_lock(
            template,
            output,
            {"nsc0": nsc0, "nslink": nslink},
        )
        by_id = {item["id"]: item for item in captured["artifacts"]}
        self.assertEqual(by_id["nsc0"]["sha256"], nscbuilder.sha256_file(nsc0))
        self.assertEqual(by_id["nslink"]["sha256"], nscbuilder.sha256_file(nslink))
        self.assertEqual(by_id["llvm-c-library"]["sha256"], original_llvm_hash)
        self.assertEqual(json.loads(template.read_text())["artifacts"][0]["sha256"], "0" * 64)
        self.assertTrue(output.is_file())

    def test_environment_verifier_rejects_pending_hash(self) -> None:
        artifact = self.root / "artifact.bin"
        artifact.write_bytes(b"pending")
        lock = {
            "schema": "nativesharp-bootstrap-lock",
            "version": 1,
            "target": {"host": "test"},
            "artifacts": [
                {
                    "id": "fixture",
                    "path": "${FIXTURE}",
                    "state": "pending",
                    "sha256": None,
                }
            ],
        }
        report = verify_environment.verify(lock, {"FIXTURE": str(artifact)})
        self.assertEqual(report["status"], "failed")
        self.assertEqual(len(report["problems"]), 2)

    def test_cleanup_requires_marker_and_only_selects_named_children(self) -> None:
        state = self.root / "state"
        state.mkdir()
        with self.assertRaises(ValueError):
            clean_selfhost.cleanup_targets(state)
        clean_selfhost.initialize(state)
        generated = state / "build"
        generated.mkdir()
        preserved = state / "sources"
        preserved.mkdir()
        self.assertEqual(clean_selfhost.cleanup_targets(state), [generated.resolve()])
        self.assertTrue(preserved.is_dir())

    def test_structured_report_matches_public_v1_contract(self) -> None:
        report = nscbuilder.structured_run_report(
            {
                "id": "fixture-run",
                "started_at": "2026-09-19T00:00:00Z",
                "finished_at": "2026-09-19T00:00:01Z",
                "status": "passed",
                "plan": {"operation": "baseline"},
                "stages": [{"name": "fixture", "status": "passed"}],
                "cases": [],
                "failed": 0,
                "total_seconds": 1.0,
                "failures": [],
            }
        )
        self.assertEqual(report["schema"], "nativesharp-selfhost-report")
        self.assertEqual(report["version"], 1)
        self.assertEqual(report["run_id"], "fixture-run")
        self.assertEqual(report["inputs"]["operation"], "baseline")
        self.assertEqual(report["problems"], [])

    def test_llvm_c_contract_freezes_inventory_and_binding_sources(self) -> None:
        contract = json.loads(
            (ROOT / "selfhost/contracts/llvm-c-api.contract.json").read_text(encoding="utf-8")
        )
        self.assertEqual(contract["schema"], "nativesharp-llvm-c-api-contract")
        self.assertEqual(contract["supported_llvm_major"], 23)
        self.assertEqual(len(contract["nsc0_import_symbols"]), 219)
        self.assertEqual(len(contract["bootstrap_symbols"]), 57)
        signature_source = ROOT / contract["signature_source"]
        enum_source = ROOT / contract["enum_source"]
        declared = verify_llvm_c_contract.declared_binding_symbols(signature_source)
        self.assertEqual(len(declared), 127)
        self.assertTrue(set(contract["bootstrap_symbols"]).issubset(declared))
        audited = set(contract["nsc0_import_symbols"]) | set(contract["bootstrap_extension_symbols"])
        self.assertTrue(set(declared).issubset(audited))
        self.assertEqual(
            verify_llvm_c_contract.digest(signature_source),
            contract["signature_source_sha256"],
        )
        self.assertEqual(
            verify_llvm_c_contract.digest(enum_source),
            contract["enum_source_sha256"],
        )

    def test_sh45_low_level_contract_covers_profiles_linkers_and_negative_boundaries(self) -> None:
        contract = json.loads(
            (ROOT / "selfhost/contracts/low-level-parity.contract.json").read_text(
                encoding="utf-8"
            )
        )
        self.assertEqual(contract["schema"], "nativesharp-selfhost-low-level-parity-contract")
        self.assertEqual(contract["profiles"], ["O0", "O1", "O2", "O3"])
        self.assertEqual(contract["linkers"], ["lld", "nslink"])
        self.assertEqual(len(contract["positive_projects"]), 5)
        self.assertEqual(len(contract["negative_projects"]), 4)
        self.assertIn("__chkstk", contract["imports"]["ntdll.dll"])
        self.assertIn("lock", contract["atomic_disassembly_fragments"])
        diagnostics = {case["diagnostic"] for case in contract["negative_projects"]}
        self.assertTrue(any("destroy requires" in text for text in diagnostics))
        self.assertTrue(any("bitfield width" in text for text in diagnostics))
        self.assertIn('kind=Destructor name="Cell.Cell"', contract["metadata_fragments"])
        for project in contract["positive_projects"]:
            self.assertTrue((ROOT / project).is_file(), project)
        for case in contract["negative_projects"]:
            self.assertTrue((ROOT / case["project"]).is_file(), case["project"])

    def test_sh46_interop_contract_covers_native_boundaries_and_products(self) -> None:
        contract = json.loads(
            (ROOT / "selfhost/contracts/interop-parity.contract.json").read_text(
                encoding="utf-8"
            )
        )
        self.assertEqual(contract["schema"], "nativesharp-selfhost-interop-parity-contract")
        self.assertEqual(contract["profiles"], ["O0", "O1", "O2", "O3"])
        self.assertEqual(contract["linkers"], ["lld", "nslink"])
        self.assertEqual(contract["expected_executions_per_profile"], 4)
        for boundary in ("C", "C++", "COM-vtable", "callback", "aggregate-parameter",
                         "aggregate-return", "varargs", "vectorcall", "DLL"):
            self.assertIn(boundary, contract["boundaries"])
        for key in ("project", "host_c", "host_cpp", "dll_consumer"):
            self.assertTrue((ROOT / contract[key]).is_file(), contract[key])

    def test_runtime_bootstrap_contract_names_external_artifacts(self) -> None:
        contract = json.loads(
            (ROOT / "selfhost/contracts/runtime-bootstrap.contract.json").read_text(
                encoding="utf-8"
            )
        )
        self.assertEqual(contract["schema"], "nativesharp-runtime-bootstrap-contract")
        self.assertEqual(contract["runtime_abi"], "1.0")
        self.assertEqual(contract["runtime_symbol_set"], 2)
        self.assertEqual(len(contract["required_symbols"]), 13)
        self.assertTrue(contract["runtime_library"].endswith(".lib"))
        self.assertTrue(contract["runtime_abi_contract"].endswith(".abi"))

    def test_sh04_diagnostics_map_freezes_codes_and_exit_codes(self) -> None:
        contract = verify_nsc_next_skeleton.load_contract(
            ROOT / "selfhost/contracts/diagnostics.map.json"
        )
        self.assertEqual(
            [item["code"] for item in contract["diagnostics"]],
            ["NSC0001", "NSC0002"],
        )
        self.assertEqual(
            [item["exit_code"] for item in contract["diagnostics"]],
            [1, 2],
        )

    def test_sh04_stable_hash_matches_native_implementation(self) -> None:
        self.assertEqual(
            verify_nsc_next_skeleton.fnv1a64("first\nsecond\nthird\n"),
            10519248498540176672,
        )

    def test_sh05_frontend_contract_freezes_differential_matrix(self) -> None:
        contract = verify_frontend.load_contract(
            ROOT / "selfhost/contracts/frontend.contract.json"
        )
        self.assertEqual(len(contract["ast_cases"]), 16)
        self.assertEqual(len(contract["recovery"]["cases"]), 5)
        self.assertIn(
            "selfhost/tests/profile/negative/comptime.ns",
            contract["token_exclusions"],
        )
        self.assertEqual(
            contract["corpus_roots"],
            ["book_examples", "examples", "lib", "selfhost/nsc", "selfhost/tests", "tests"],
        )

    def test_selfhost_analysis_corpora_ignore_generated_build_trees(self) -> None:
        corpus_roots = ["book_examples", "examples", "lib", "selfhost/nsc", "selfhost/tests", "tests"]
        expected: list[Path] = []
        for relative_root in corpus_roots:
            directory = self.root / relative_root
            directory.mkdir(parents=True)
            source = directory / "Canonical.ns"
            source.write_text("int Main() { return 0; }\n", encoding="utf-8")
            expected.append(source)
        generated = self.root / "build-any-name" / "selfhost-sh44" / "staged" / "ruta-ñ-日本" / "Generated.ns"
        generated.parent.mkdir(parents=True)
        generated.write_text("int Main() { return 1; }\n", encoding="utf-8")

        self.assertEqual(verify_frontend.source_corpus(self.root, corpus_roots, set()), expected)
        self.assertEqual(verify_semantics.source_corpus(self.root, corpus_roots), expected)
        self.assertEqual(verify_ir.source_corpus(self.root, corpus_roots), expected)

    def test_sh05_ast_canonicalization_ignores_only_blank_lines(self) -> None:
        legacy = b'node "\r\n\r\n  mov rax, rbx\r\n"\r\n  child\r\n'
        candidate = b'node "\n  mov rax, rbx\n"\n  child\n'
        self.assertEqual(
            verify_frontend.canonical_ast(legacy),
            verify_frontend.canonical_ast(candidate),
        )

    def test_sh05_line_canonicalization_is_host_independent(self) -> None:
        self.assertEqual(
            verify_frontend.canonical_lines(b"one\r\ntwo\rthree\n"),
            b"one\ntwo\nthree\n",
        )

    def test_sh06_semantic_contract_freezes_differential_and_invariants(self) -> None:
        contract = verify_semantics.load_contract(
            ROOT / "selfhost/contracts/semantic.contract.json"
        )
        self.assertEqual(len(contract["differential_cases"]), 12)
        self.assertEqual(contract["actions"], ["--dump-symbols", "--dump-sema"])
        self.assertGreaterEqual(len(contract["invariants"]), 6)
        self.assertEqual(len(contract["corpus_roots"]), 6)

    def test_sh06_line_canonicalization_is_host_independent(self) -> None:
        self.assertEqual(
            verify_semantics.canonical_lines(b"one\r\ntwo\rthree\n"),
            b"one\ntwo\nthree\n",
        )

    def test_sh07_ir_contract_freezes_all_boundaries_and_levels(self) -> None:
        contract = verify_ir.load_contract(ROOT / "selfhost/contracts/ir.contract.json")
        self.assertEqual(contract["optimizations"], ["O0", "O1", "O2", "O3"])
        self.assertEqual(len(contract["stages"]), 6)
        self.assertEqual(contract["stages"][0]["dump"], "--dump-hir")
        self.assertEqual(contract["stages"][4]["dump"], "--dump-ssa")
        self.assertGreaterEqual(contract["corpus_minimum_sources"], 480)
        self.assertGreaterEqual(contract["oracle_ssa_minimum_sources"], 20)
        self.assertEqual(len(contract["corpus_roots"]), 6)

    def test_sh07_line_canonicalization_is_host_independent(self) -> None:
        self.assertEqual(verify_ir.canonical_lines(b"one\r\ntwo\rthree\n"), b"one\ntwo\nthree\n")

    def test_sh07_reducer_keeps_first_divergence(self) -> None:
        source = "header\nkeep divergence\ntail-a\ntail-b\n"
        reduced = verify_ir.reduce_lines(source, lambda value: "divergence" in value)
        self.assertIn("divergence", reduced)
        self.assertLess(len(reduced), len(source))

    def test_sh08_backend_contract_freezes_profiles_artifacts_and_linkers(self) -> None:
        contract = json.loads((ROOT / "selfhost/contracts/llvm-backend.contract.json").read_text())
        self.assertEqual(contract["schema"], "nativesharp-selfhost-llvm-backend-contract")
        self.assertEqual(contract["optimizations"], ["O0", "O1", "O2", "O3"])
        self.assertEqual(contract["artifacts"], ["object", "assembly"])
        self.assertEqual(contract["linkers"], ["lld-link", "NSLink"])
        self.assertEqual(contract["expected_exit_code"], 42)
        self.assertEqual(len(contract["covered_work_items"]), 14)

    def test_sh08_verifier_is_source_tree_read_only(self) -> None:
        source = (ROOT / "selfhost/bootstrap/verify_llvm_backend.py").read_text()
        self.assertIn("backend_source_sha256", source)
        self.assertIn("args.report.write_text", source)
        self.assertNotIn("source_root.write_text", source)

    def test_sh09_project_contract_freezes_driver_matrix(self) -> None:
        contract = json.loads((ROOT / "selfhost/contracts/project-driver.contract.json").read_text())
        self.assertEqual(contract["schema"], "nativesharp-selfhost-project-driver-contract")
        self.assertEqual(contract["optimizations"], ["O0", "O1", "O2", "O3"])
        self.assertEqual(contract["linkers"], ["lld-link", "NSLink"])
        self.assertEqual(contract["metadata_versions"], [1, 2])
        self.assertEqual(len(contract["work_items"]), 14)

    def test_sh10_first_generation_contract_freezes_clean_build_matrix(self) -> None:
        contract = json.loads((ROOT / "selfhost/contracts/first-generation.contract.json").read_text())
        self.assertEqual(contract["schema"], "nativesharp-selfhost-first-generation-contract")
        self.assertEqual(contract["generation"], "nsc1")
        self.assertEqual(contract["built_by"], "nsc0")
        self.assertEqual(contract["optimizations"], ["O0", "O1", "O2", "O3"])
        self.assertEqual(contract["linkers"], ["lld-link", "NSLink"])
        self.assertEqual(len(contract["source_roots"]), 5)
        self.assertEqual(len(contract["work_items"]), 12)

    def test_sh10_reproducible_link_is_requested_without_binary_rewriting(self) -> None:
        source = (ROOT / "src/Linker/LldLinker.cpp").read_text(encoding="utf-8")
        self.assertGreaterEqual(source.count('\"/Brepro\"'), 2)
        verifier = (ROOT / "selfhost/bootstrap/verify_first_generation.py").read_text()
        self.assertIn("byte_identical_clean_builds", verifier)
        self.assertNotIn("write_bytes", verifier)

    def test_sh41_contract_freezes_real_source_derived_generation(self) -> None:
        contract = json.loads(
            (ROOT / "selfhost/contracts/self-hosted-generation.contract.json").read_text()
        )
        self.assertEqual(contract["schema"], "nativesharp-selfhost-generation-contract")
        self.assertEqual(contract["generation"], "nsc2")
        self.assertEqual(contract["built_by"], "nsc1")
        self.assertEqual(contract["version"], 2)
        self.assertEqual(contract["gate"], "SH41")
        self.assertEqual(contract["generation_method"], "source-derived-llvm-object")
        self.assertTrue(contract["source_derived_codegen"])
        self.assertFalse(contract["image_replay"])
        self.assertEqual(contract["optimizations"], ["O0", "O1", "O2", "O3"])
        self.assertEqual(contract["linkers"], ["lld-link", "NSLink"])
        self.assertEqual(len(contract["work_items"]), 11)

    def test_sh41_generation_audits_seed_isolation_and_fixed_point(self) -> None:
        verifier = (ROOT / "selfhost/bootstrap/verify_self_hosted_generation.py").read_text()
        self.assertIn("forbidden_seed_invocations", verifier)
        self.assertIn('"nsc0_invocations_during_generation": 0', verifier)
        self.assertIn('"source_derived_codegen": True', verifier)
        self.assertIn("nsc2_object.read_bytes() == nsc3_object.read_bytes()", verifier)
        self.assertIn("PROCESS_AUDIT_INTERVAL_SECONDS = 0.010", verifier)
        self.assertNotIn("time.sleep(0.001)", verifier)
        self.assertNotIn("source-attested-image-replay", verifier)

    def test_sh44_local_environment_matrix_is_a_complete_gate(self) -> None:
        contract = json.loads(
            (ROOT / "selfhost/contracts/cross-machine-determinism.contract.json").read_text()
        )
        self.assertEqual(contract["required_machines"], 1)
        self.assertEqual(len(contract["variants"]), 4)
        self.assertEqual(contract["optimizations"], ["O0", "O1", "O2", "O3"])
        self.assertEqual(contract["linkers"], ["lld", "nslink"])
        verifier = (ROOT / "selfhost/bootstrap/verify_cross_machine_determinism.py").read_text()
        self.assertIn("requirement_met = peer is not None or required_machines == 1", verifier)
        source_graph = (ROOT / "selfhost/nsc/Projects/SourceGraph.ns").read_text()
        self.assertIn("File.Exists(normalized)", source_graph)
        self.assertIn("ProjectLoader.Load(normalized)", source_graph)
        self.assertIn("Path.GetDirectoryName(normalized)", source_graph)

    def test_sh12_fixed_point_contract_separates_local_and_cross_machine_proof(self) -> None:
        contract = json.loads((ROOT / "selfhost/contracts/fixed-point.contract.json").read_text())
        self.assertEqual(contract["schema"], "nativesharp-selfhost-fixed-point-contract")
        self.assertEqual(contract["repetitions"], 3)
        self.assertEqual(contract["required_machines"], 2)
        self.assertEqual(contract["normalization"], [])
        self.assertFalse(contract["source_derived_codegen"])
        verifier = (ROOT / "selfhost/bootstrap/verify_fixed_point.py").read_text()
        self.assertIn('"image_fixed_point": True', verifier)
        self.assertIn('"cross_machine_verified": cross_machine', verifier)
        self.assertIn("NS2-SH-157 requires", verifier)

    def test_sh13_robustness_contract_has_separate_measured_budgets(self) -> None:
        contract = json.loads((ROOT / "selfhost/contracts/robustness.contract.json").read_text())
        self.assertEqual(contract["schema"], "nativesharp-selfhost-robustness-contract")
        self.assertEqual(contract["profiles"], ["O0", "O1", "O2", "O3"])
        self.assertGreaterEqual(contract["fuzz_cases"], 24)
        self.assertIn("maximum_startup_ratio", contract)
        self.assertIn("maximum_compile_ratio", contract)
        self.assertEqual(len(contract["work_items"]), 12)

    def test_sh14_promotion_contract_keeps_seed_until_all_requirements_land(self) -> None:
        contract = json.loads((ROOT / "selfhost/contracts/promotion-readiness.contract.json").read_text())
        self.assertEqual(contract["required_landed_gates"], ["SH10", "SH11", "SH12", "SH13"])
        self.assertTrue(contract["required_source_derived_codegen"])
        self.assertEqual(contract["fallback_compiler"], "nsc0")
        verifier = (ROOT / "selfhost/bootstrap/verify_promotion_readiness.py").read_text()
        self.assertIn('"withhold"', verifier)
        self.assertIn('"promotion_allowed": promoted', verifier)

    def test_tanda16_closure_freezes_commits_and_safe_nonpromotion(self) -> None:
        contract = json.loads((ROOT / "selfhost/contracts/closure.contract.json").read_text())
        self.assertEqual(contract["tandas"], list(range(7, 17)))
        self.assertEqual(contract["required_safe_default"], "nsc0")
        self.assertFalse(contract["required_promotion_allowed"])
        self.assertEqual(contract["expected_gate_states"]["SH11"], "LANDED")
        self.assertEqual(contract["expected_gate_states"]["SH12"], "OPEN")
        self.assertEqual(contract["known_commits"]["15"], "8f065e892")
        verifier = (ROOT / "selfhost/bootstrap/verify_closure.py").read_text()
        self.assertIn("SELFHOST-CROSS-MACHINE", verifier)
        self.assertIn("SELFHOST-SOURCE-DERIVED-CODEGEN", verifier)

    def test_tanda17_retires_image_replay_and_fails_closed(self) -> None:
        contract = json.loads((ROOT / "selfhost/contracts/image-replay-retirement.contract.json").read_text())
        self.assertFalse(contract["image_replay_allowed"])
        self.assertEqual(contract["blocked_exit_code"], 78)
        builder = (ROOT / "selfhost/nsc/Bootstrap/GenerationBuilder.ns").read_text()
        self.assertNotIn("File.Copy(", builder)
        self.assertIn("source-derived-codegen-required", builder)
        self.assertIn("return 78;", builder)

    def test_tanda18_backend_is_parameterized_by_the_parsed_source(self) -> None:
        contract = json.loads((ROOT / "selfhost/contracts/source-derived-backend.contract.json").read_text())
        self.assertEqual(contract["gate"], "SH16")
        self.assertEqual(contract["expected_exit_codes"], [17, 42])
        backend = (ROOT / "selfhost/nsc/CodeGen/LlvmBackend.ns").read_text()
        self.assertIn("SyntaxTree tree, SemanticModel semantic", backend)
        self.assertIn("EmitScalarExpression", backend)
        self.assertNotIn("void* six =", backend)

    def test_tanda19_source_functions_use_llvm_parameters_and_calls(self) -> None:
        contract = json.loads((ROOT / "selfhost/contracts/source-functions.contract.json").read_text())
        self.assertEqual(contract["gate"], "SH17")
        self.assertEqual(contract["profiles"], ["O0", "O1", "O2", "O3"])
        backend = (ROOT / "selfhost/nsc/CodeGen/LlvmBackend.ns").read_text()
        for api in ("LLVMGetParam", "LLVMGetNamedFunction", "LLVMBuildCall2"):
            self.assertIn(api, backend)
        self.assertIn("semantic.Fact(syntaxId).SymbolId()", backend)

    def test_tanda20_source_control_flow_uses_semantic_locals_and_llvm_blocks(self) -> None:
        contract = json.loads((ROOT / "selfhost/contracts/source-control-flow.contract.json").read_text())
        self.assertEqual(contract["gate"], "SH18")
        self.assertEqual(contract["profiles"], ["O0", "O1", "O2", "O3"])
        backend = (ROOT / "selfhost/nsc/CodeGen/LlvmBackend.ns").read_text()
        for api in ("LLVMBuildAlloca", "LLVMBuildLoad2", "LLVMBuildStore", "LLVMBuildCondBr"):
            self.assertIn(api, backend)
        self.assertIn("semantic.NodeSymbol(syntaxId)", backend)

    def test_tanda21_source_loop_control_uses_explicit_loop_targets(self) -> None:
        contract = json.loads((ROOT / "selfhost/contracts/source-loop-control.contract.json").read_text())
        self.assertEqual(contract["gate"], "SH19")
        self.assertEqual(contract["profiles"], ["O0", "O1", "O2", "O3"])
        backend = (ROOT / "selfhost/nsc/CodeGen/LlvmBackend.ns").read_text()
        for marker in ("ForStatement", "DoWhileStatement", "BreakStatement", "ContinueStatement"):
            self.assertIn(marker, backend)
        self.assertIn("PushLoop", backend)
        self.assertIn("PopLoop", backend)
        self.assertIn("EmitEntryAlloca", backend)

    def test_tanda22_source_bool_void_uses_typed_scalars(self) -> None:
        contract = json.loads((ROOT / "selfhost/contracts/source-bool-void.contract.json").read_text())
        self.assertEqual(contract["gate"], "SH20")
        backend = (ROOT / "selfhost/nsc/CodeGen/LlvmBackend.ns").read_text()
        for marker in ("BooleanLiteralExpression", "LLVMBuildNot", "LLVMBuildRetVoid", "LLVMGlobalGetValueType"):
            self.assertIn(marker, backend)
        self.assertIn("FindLocalType", backend)

    def test_tanda23_source_expressions_use_short_circuit_phi_and_lvalue_updates(self) -> None:
        contract = json.loads((ROOT / "selfhost/contracts/source-expressions.contract.json").read_text())
        self.assertEqual(contract["gate"], "SH21")
        self.assertEqual(contract["profiles"], ["O0", "O1", "O2", "O3"])
        backend = (ROOT / "selfhost/nsc/CodeGen/LlvmBackend.ns").read_text()
        for marker in ("EmitIntegerOperation", "PostfixExpression", "LLVMBuildPhi", "LLVMAddIncoming"):
            self.assertIn(marker, backend)
        self.assertIn("TokenKind.AmpAmp", backend)
        self.assertIn("TokenKind.PipePipe", backend)

    def test_tanda24_integer_widths_use_explicit_signed_conversions(self) -> None:
        contract = json.loads((ROOT / "selfhost/contracts/source-integer-widths.contract.json").read_text())
        self.assertEqual(contract["gate"], "SH22")
        backend = (ROOT / "selfhost/nsc/CodeGen/LlvmBackend.ns").read_text()
        for marker in ("IntegerBits", "UnsignedInteger", "ConvertScalar", "ApplyConversion"):
            self.assertIn(marker, backend)
        for api in ("LLVMBuildTrunc", "LLVMBuildSExt", "LLVMBuildZExt", "LLVMBuildUDiv", "LLVMBuildLShr"):
            self.assertIn(api, backend)

    def test_tanda25_pointers_use_general_lvalues_and_opaque_addresses(self) -> None:
        contract = json.loads((ROOT / "selfhost/contracts/source-pointers.contract.json").read_text())
        self.assertEqual(contract["gate"], "SH23")
        self.assertEqual(contract["profiles"], ["O0", "O1", "O2", "O3"])
        backend = (ROOT / "selfhost/nsc/CodeGen/LlvmBackend.ns").read_text()
        for marker in ("EmitLValue", "NullLiteralExpression", "StackallocExpression", "IndexExpression"):
            self.assertIn(marker, backend)
        for api in ("LLVMBuildArrayAlloca", "LLVMBuildGEP2", "LLVMBuildLoad2", "LLVMBuildStore"):
            self.assertIn(api, backend)

    def test_tanda26_floating_scalars_use_real_operations_and_conversions(self) -> None:
        contract = json.loads((ROOT / "selfhost/contracts/source-floating-scalars.contract.json").read_text())
        self.assertEqual(contract["gate"], "SH24")
        self.assertEqual(contract["profiles"], ["O0", "O1", "O2", "O3"])
        backend = (ROOT / "selfhost/nsc/CodeGen/LlvmBackend.ns").read_text()
        for marker in ("FloatingScalar", "EmitFloatingOperation", "FloatingComparisonPredicate", "_fltused"):
            self.assertIn(marker, backend)
        for api in ("LLVMBuildFAdd", "LLVMBuildFRem", "LLVMBuildFCmp", "LLVMBuildFPExt", "LLVMBuildFPToSI"):
            self.assertIn(api, backend)

    def test_sh47_specialized_code_contract_covers_native_and_nsasm_boundaries(self) -> None:
        contract = json.loads((ROOT / "selfhost/contracts/specialized-code.contract.json").read_text())
        self.assertEqual(contract["schema"], "nativesharp-selfhost-specialized-code-contract")
        self.assertEqual(contract["profiles"], ["O0", "O1", "O2", "O3"])
        self.assertEqual(contract["linkers"], ["lld", "nslink"])
        self.assertEqual(contract["expected_executions"], 8)
        for surface in ("vectors", "matrices", "half", "packed-formats", "hardware-intrinsics",
                        "inline-assembly", "floating-point-modes", "NSAsm"):
            self.assertIn(surface, contract["surfaces"])

    def test_sh47_backend_lowers_inline_assembly_through_llvm_c(self) -> None:
        backend = (ROOT / "selfhost/nsc/CodeGen/LlvmBackend.ns").read_text()
        binding = (ROOT / "lib/NativeSharp.LLVM.C/LLVMApi.ns").read_text()
        self.assertIn("EmitInlineAssembly", backend)
        self.assertIn("InlineAssemblyConstraints", backend)
        self.assertIn("LLVMGetInlineAsm", backend)
        self.assertIn("LLVMGetInlineAsm", binding)


if __name__ == "__main__":
    unittest.main()
