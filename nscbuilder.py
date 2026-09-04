#!/usr/bin/env python3
"""GUI de construcción y pruebas para NativeSharp.

Requisitos:
    python -m pip install PySide6

Los resultados se guardan como archivos ordinarios bajo:
    <repo>\\_nscbuilder\\history\\<fecha-id>\\

No usa una base de datos. Cada corrida deja metadata.json, console.log,
report.md y logs separados por etapa y por caso.
"""

from __future__ import annotations

import argparse
import json
import os
import queue
import re
import shutil
import subprocess
import sys
import threading
import time
import traceback
import uuid
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import asdict, dataclass
from datetime import datetime
from pathlib import Path
from typing import Callable, Iterable

try:
    from PySide6.QtCore import QObject, QProcess, QSettings, QThread, QTimer, Qt, QUrl, Signal, Slot
    from PySide6.QtGui import QColor, QDesktopServices, QFontDatabase, QTextCharFormat, QTextCursor
    from PySide6.QtWidgets import (
        QAbstractItemView,
        QApplication,
        QCheckBox,
        QComboBox,
        QDockWidget,
        QFormLayout,
        QGridLayout,
        QGroupBox,
        QHBoxLayout,
        QHeaderView,
        QLabel,
        QLineEdit,
        QListWidget,
        QListWidgetItem,
        QMainWindow,
        QMessageBox,
        QPlainTextEdit,
        QProgressBar,
        QPushButton,
        QSpinBox,
        QTableWidget,
        QTableWidgetItem,
        QTabWidget,
        QTextEdit,
        QVBoxLayout,
        QWidget,
    )
except ModuleNotFoundError as exc:  # pragma: no cover - depende de la máquina del usuario
    if exc.name and exc.name.startswith("PySide6"):
        print("PySide6 no está instalado. Instálalo con: python -m pip install PySide6")
        raise SystemExit(2) from exc
    raise


APP_NAME = "NSC Builder"
ROOT = Path(__file__).resolve().parent
DEFAULT_BUILD = ROOT / "build-clang"
DEFAULT_PREFIX = Path(r"C:\native-sharp")
DEFAULT_LLVM = Path(r"C:\clang-llvm")
DEFAULT_NSLINK = Path(r"C:\native-sharp\bin\nslink.exe")
HISTORY_ROOT = ROOT / "_nscbuilder" / "history"
CASE_TIMEOUT_SECONDS = 300

INTEROP_CASES: tuple[tuple[str, str, int | None], ...] = (
    ("c1_03_function_pointer_parameter", "c1_03_function_pointer_parameter", 42),
    ("c1_04_function_pointer_return", "c1_04_function_pointer_return", 42),
    ("c1_05_raw_pointer_cast", "c1_05_raw_pointer_cast", 42),
    ("c2_11_fixed_field_array", "c2_11_fixed_field_array", 42),
    ("c2_12_union", "c2_12_union", 42),
    ("c4_ref_out_in", "c4_ref_out_in", 42),
    ("c6_struct_math", "c6_struct_math", 42),
    ("c7_definite_assignment", "c7_definite_assignment", 42),
    ("c8_type_root", "c8_type_root", 42),
    ("c1_14_callback_exception_boundary", "c1_14_callback_exception_boundary", 141),
    ("c1_13_two_libraries_same_symbol", "c1_13_two_libraries_same_symbol", None),
)

LANGUAGE_CASES: tuple[tuple[str, str, int | None], ...] = (
    ("l1_conditional", "l1_conditional", 42),
    ("l2_dowhile", "l2_dowhile", 42),
    ("l3_switch", "l3_switch", 42),
)

CC1_CASES: tuple[tuple[str, int | None], ...] = (
    ("c1_constants", 42),
    ("c2_ternary_ownership", 42),
    ("c3_dowhile_flow", 42),
    ("c4_catch_continue", 42),
    ("c5_catch_break", 42),
    ("c6_catch_return", 42),
    ("c7_finally_switch", 42),
    ("c8_loop_catch_switch", 42),
    ("c9_sizeof", 42),
    ("c10_stackalloc", 42),
    ("c11_raw_new", 42),
    ("c12_pointer_arith", 42),
    ("c13_increment", 42),
    ("c14_compound_assign", 42),
    ("c15_arrow_member", 42),
    ("c16_enum_layout", 42),
    ("c17_constant_promotion", 42),
    ("c18_stackalloc_hardening", 42),
    ("c19_sbyte", 42),
    ("c20_property_compound", 42),
    ("c21_number_parsing", 42),
    ("c22_console_and_formatting", 42),
    ("c23_views_and_lifetimes", 42),
    ("c24_binary_file_io", 42),
    ("c25_binary_roundtrip", 42),
    ("c26_driver_minimo", 42),
    ("c27_algoritmos", 42),
    ("c28_texto", 42),
    ("c29_arena", 42),
    ("c30_contratos", 42),
    ("c31_cadena_c", 42),
    ("c32_division_por_cero", 129),
    ("c33_hilos_join", 42),
    ("c34_enum_nombre", 42),
    ("c35_sobrecarga_generica", 42),
    ("c36_utf8", 42),
    ("c37_contrato_de_layout", 42),
    ("c38_enum_ancho", 42),
    ("c39_enum_nombre_o2", 42),
)

EXAMPLE_CASES: tuple[tuple[str, str, int | None], ...] = (
    ("examples/project_stage56_c1/Stage56C1InteropCompletion.nsproj", "project_stage56_c1", 0),
    (
        "examples/project_stage56_c2c4/Stage56C2C4LanguageCompletion.nsproj",
        "project_stage56_c2c4",
        0,
    ),
    ("examples/project_stage56_c6/Stage56C6StructMath.nsproj", "project_stage56_c6", 0),
    ("examples/project_stage56_c7/project_stage56_c7.nsproj", "project_stage56_c7", 0),
    ("examples/project_stage56_c8/project_stage56_c8.nsproj", "project_stage56_c8", 0),
    ("examples/project_demo_ventana/DemoVentana.nsproj", "project_demo_ventana", None),
)

TRIAGE_CASES: tuple[tuple[str, str], ...] = (
    ("tests/bug_triage/b1_this_copy/b1_this_copy.nsproj", "b1_this_copy"),
    ("tests/bug_triage/b2_operator_null/b2_operator_null.nsproj", "b2_operator_null"),
    ("tests/bug_triage/b3_ctor_aggregate/b3_ctor_aggregate.nsproj", "b3_ctor_aggregate"),
    ("tests/bug_triage/b4_this_by_value/b4_this_by_value.nsproj", "b4_this_by_value"),
    (
        "tests/bug_triage/b5_ctor_owned_field_array/b5_ctor_owned_field_array.nsproj",
        "b5_ctor_owned_field_array",
    ),
    (
        "tests/bug_triage/b6_delegate_projectref/app/b6_delegate_projectref.nsproj",
        "b6_delegate_projectref",
    ),
    ("tests/bug_triage/b7_duplicate_extern_c/b7_duplicate_extern_c.nsproj", "b7_duplicate_extern_c"),
    ("tests/bug_triage/b8_shift_folding/b8_shift_folding.nsproj", "b8_shift_folding"),
    ("tests/bug_triage/b9_enum_estrecho_o2/b9_enum_estrecho_o2.nsproj", "b9_enum_estrecho_o2"),
)


@dataclass(frozen=True)
class CaseSpec:
    project: str
    label: str
    expected: int | None
    source: str


@dataclass
class RunPlan:
    root: str
    build_dir: str
    prefix: str
    llvm_root: str
    nslink_exe: str
    build_mode: str
    build_target: str
    build_runner: str
    linkers: list[str]
    optimizations: list[str]
    jobs: int
    run_build: bool
    sync_libraries: bool
    run_ctest: bool
    run_cases: bool
    run_triage: bool
    keep_link_temps: bool
    selected_ctests: list[str]
    selected_cases: list[str]


@dataclass
class ProcessResult:
    exit_code: int
    seconds: float
    output: str
    error: str = ""
    cancelled: bool = False
    timed_out: bool = False


def utc_stamp() -> str:
    return datetime.now().astimezone().isoformat(timespec="seconds")


def safe_name(text: str) -> str:
    cleaned = re.sub(r"[^A-Za-z0-9_.-]+", "_", text).strip("._")
    return cleaned or "item"


def command_text(command: Iterable[str]) -> str:
    return subprocess.list2cmdline([str(part) for part in command])


def write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, indent=2, ensure_ascii=False), encoding="utf-8")
    temporary.replace(path)


def import_case_table(root: Path) -> list[CaseSpec]:
    result = [
        CaseSpec(
            f"tests/interop_contract/{directory}/{directory}.nsproj",
            label,
            expected,
            "nscbuilder.py",
        )
        for directory, label, expected in INTEROP_CASES
    ]
    result.extend(
        CaseSpec(f"tests/language/{directory}/{directory}.nsproj", label, expected, "nscbuilder.py")
        for directory, label, expected in LANGUAGE_CASES
    )
    result.extend(
        CaseSpec(f"tests/cc1/{directory}/{directory}.nsproj", f"cc1_{directory}", expected, "nscbuilder.py")
        for directory, expected in CC1_CASES
    )
    result.extend(CaseSpec(project, label, expected, "nscbuilder.py") for project, label, expected in EXAMPLE_CASES)

    # El registro C++ permite descubrir fixtures CC1 recién añadidas sin delegar en otro runner.
    registry = root / "tests" / "unit" / "Cc1CorrectnessTests.cpp"
    if registry.is_file():
        text = registry.read_text(encoding="utf-8", errors="replace")
        pattern = re.compile(
            r"checkFixture\s*\(\s*tests\s*,\s*root\s*,\s*"
            r'"([^"]+)"\s*,\s*"([^"]+)"\s*,\s*(-?\d+)\s*\)',
            re.DOTALL,
        )
        known_projects = {case.project.replace("\\", "/").lower() for case in result}
        for project, label, expected in pattern.findall(text):
            key = project.replace("\\", "/").lower()
            if key not in known_projects:
                result.append(CaseSpec(project, label, int(expected), "Cc1CorrectnessTests.cpp"))
                known_projects.add(key)

    return result


def ctest_group(name: str) -> str:
    lowered = name.casefold()
    stage = re.search(r"(?:^|\.)stage[-_.]?(\d+)", lowered)
    if stage:
        number = int(stage.group(1))
        if number <= 34:
            return "Stages 25–34"
        if number <= 44:
            return "Stages 35–44"
        if number <= 49:
            return "Stages 45–49"
        if number <= 54:
            return "Stages 50–54"
        return "Stages 55–59"
    if ".prestage" in lowered:
        return "Stages 55–59"
    if ".cc1" in lowered or lowered.startswith("nsc.cc1"):
        return "CC1"
    if ".integration" in lowered:
        return "Integración"
    if ".invalid" in lowered:
        return "Entradas inválidas"
    if ".dump" in lowered or ".verify" in lowered:
        return "Driver / IR"
    if any(
        token in lowered
        for token in (
            "source-manager",
            "diagnostic",
            "lexer",
            "parser",
            "syntax",
            "declaration",
            "partial-class",
            "constant-folder",
        )
    ):
        return "Frontend base"
    if any(
        token in lowered
        for token in (
            "project-",
            "compilation-model",
            "semantics",
            "macro-a-oop",
        )
    ):
        return "Modelo semántico"
    if any(token in lowered for token in ("hir", "mir", "cfg", "ssa", "dominance")):
        return "HIR / MIR / CFG"
    if any(token in lowered for token in ("llvm", "optimization", "pass-manager", "codegen")):
        return "LLVM / optimización"
    if any(token in lowered for token in ("driver", "target", "runtime", "windows", "linker", "emit-exe")):
        return "Driver / plataforma"
    return "Otros"


def case_group(case: CaseSpec) -> str:
    project = case.project.replace("\\", "/").casefold()
    if project.startswith("tests/cc1/"):
        return "CC1"
    if project.startswith("tests/interop_contract/"):
        return "Interop"
    if project.startswith("tests/language/"):
        return "Lenguaje"
    if project.startswith("examples/"):
        return "Ejemplos"
    return "Otros"


class PipelineWorker(QObject):
    output = Signal(str, str, str, str)
    stage_started = Signal(str)
    stage_finished = Signal(str, str, float, int)
    failure_found = Signal(dict)
    progress = Signal(int, int)
    finished = Signal(dict)

    def __init__(self, plan: RunPlan, cases: list[CaseSpec]) -> None:
        super().__init__()
        self.plan = plan
        self.cases = cases
        self.cancel_event = threading.Event()
        self.process_lock = threading.Lock()
        self.processes: dict[int, subprocess.Popen[str]] = {}
        self.console_lock = threading.Lock()
        self.run_dir: Path | None = None
        self.console_path: Path | None = None
        self.metadata: dict = {}
        self.stage_rows: list[dict] = []
        self.case_rows: list[dict] = []
        self.failure_rows: list[dict] = []
        self.failed = 0
        self.completed_units = 0
        self.total_units = 1

    def cancel(self) -> None:
        self.cancel_event.set()
        self._emit("Cancelación solicitada; terminando procesos activos...", "warn")
        with self.process_lock:
            processes = list(self.processes.values())
        for process in processes:
            self._terminate_tree(process)

    def _emit(
        self,
        text: str,
        level: str = "info",
        channel: str = "General",
        optimization: str = "General",
    ) -> None:
        line = text.rstrip("\r\n")
        if not line:
            return
        stamped = f"[{datetime.now().strftime('%H:%M:%S')}] {line}"
        with self.console_lock:
            if self.console_path is not None:
                with self.console_path.open("a", encoding="utf-8", errors="replace") as handle:
                    handle.write(stamped + "\n")
            if self.run_dir is not None:
                channel_path = (
                    self.run_dir
                    / safe_name(channel)
                    / safe_name(optimization)
                    / "console.log"
                )
                channel_path.parent.mkdir(parents=True, exist_ok=True)
                with channel_path.open("a", encoding="utf-8", errors="replace") as handle:
                    handle.write(stamped + "\n")
        self.output.emit(stamped, level, channel, optimization)

    def _report_failure(self, failure: dict) -> None:
        row = dict(failure)
        row.setdefault("linker", "General")
        row.setdefault("optimization", "General")
        self.failure_rows.append(row)
        self.metadata["failures"] = self.failure_rows
        self._save_metadata()
        self.failure_found.emit(row)

    def _save_metadata(self) -> None:
        if self.run_dir is not None:
            write_json(self.run_dir / "metadata.json", self.metadata)

    def _register_process(self, process: subprocess.Popen[str]) -> None:
        with self.process_lock:
            self.processes[process.pid] = process

    def _unregister_process(self, process: subprocess.Popen[str]) -> None:
        with self.process_lock:
            self.processes.pop(process.pid, None)

    @staticmethod
    def _terminate_tree(process: subprocess.Popen[str]) -> None:
        if process.poll() is not None:
            return
        try:
            if os.name == "nt":
                flags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
                subprocess.run(
                    ["taskkill", "/PID", str(process.pid), "/T", "/F"],
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL,
                    creationflags=flags,
                    timeout=10,
                )
            else:
                process.terminate()
        except (OSError, subprocess.SubprocessError):
            try:
                process.kill()
            except OSError:
                pass

    def _run_process(
        self,
        command: list[str],
        cwd: Path,
        env: dict[str, str],
        log_path: Path,
        timeout: int | None = None,
        prefix: str = "",
        channel: str = "General",
        optimization: str = "General",
    ) -> ProcessResult:
        started = time.monotonic()
        lines = [f"$ {command_text(command)}"]
        self._emit(
            (prefix + " " if prefix else "") + lines[0],
            "command",
            channel,
            optimization,
        )
        flags = 0
        if os.name == "nt":
            flags = getattr(subprocess, "CREATE_NO_WINDOW", 0) | getattr(
                subprocess, "CREATE_NEW_PROCESS_GROUP", 0
            )
        try:
            process = subprocess.Popen(
                command,
                cwd=str(cwd),
                env=env,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                encoding="utf-8",
                errors="replace",
                bufsize=1,
                creationflags=flags,
            )
        except OSError as exc:
            error = f"No se pudo iniciar el proceso: {exc!r}"
            lines.append(error)
            log_path.parent.mkdir(parents=True, exist_ok=True)
            log_path.write_text("\n".join(lines) + "\n", encoding="utf-8", errors="replace")
            self._emit(
                (prefix + " " if prefix else "") + error,
                "error",
                channel,
                optimization,
            )
            return ProcessResult(-1, time.monotonic() - started, "\n".join(lines), error=error)

        self._register_process(process)
        pending: queue.Queue[str] = queue.Queue()

        def read_output() -> None:
            assert process.stdout is not None
            try:
                for raw in process.stdout:
                    pending.put(raw.rstrip("\r\n"))
            finally:
                process.stdout.close()

        reader = threading.Thread(target=read_output, daemon=True)
        reader.start()
        timed_out = False
        cancelled = False
        try:
            while process.poll() is None:
                while True:
                    try:
                        line = pending.get_nowait()
                    except queue.Empty:
                        break
                    lines.append(line)
                    self._emit(
                        (prefix + " " if prefix else "") + line,
                        "info",
                        channel,
                        optimization,
                    )
                if self.cancel_event.is_set():
                    cancelled = True
                    self._terminate_tree(process)
                    break
                if timeout is not None and time.monotonic() - started > timeout:
                    timed_out = True
                    self._terminate_tree(process)
                    break
                time.sleep(0.05)
            try:
                process.wait(timeout=15)
            except subprocess.TimeoutExpired:
                self._terminate_tree(process)
            reader.join(timeout=2)
            while True:
                try:
                    line = pending.get_nowait()
                except queue.Empty:
                    break
                lines.append(line)
                self._emit(
                    (prefix + " " if prefix else "") + line,
                    "info",
                    channel,
                    optimization,
                )
        finally:
            self._unregister_process(process)

        elapsed = time.monotonic() - started
        exit_code = process.returncode if process.returncode is not None else -1
        if timed_out:
            error = f"Timeout después de {timeout} s"
        elif cancelled:
            error = "Cancelado"
        else:
            error = ""
        if error:
            lines.append(error)
        lines.append(f"[exit {exit_code}] [{elapsed:.3f} s]")
        log_path.parent.mkdir(parents=True, exist_ok=True)
        log_path.write_text("\n".join(lines) + "\n", encoding="utf-8", errors="replace")
        return ProcessResult(
            exit_code,
            elapsed,
            "\n".join(lines),
            error=error,
            cancelled=cancelled,
            timed_out=timed_out,
        )

    def _base_environment(self, linker: str | None = None) -> dict[str, str]:
        env = dict(os.environ)
        env.pop("NSC_KEEP_LINK_TEMPS", None)
        env.pop("NSC_LLD_LINK", None)
        prefix = Path(self.plan.prefix)
        env["NS_ROOT"] = str(prefix)
        env["NSC_EXE"] = str(prefix / "bin" / "nsc.exe")
        env["NS_LIB_ROOT"] = str(prefix / "lib")
        env["NS_LIB_READY"] = "1"
        if linker == "lld-link":
            env["NSC_LLD_LINK"] = str(Path(self.plan.llvm_root) / "bin" / "lld-link.exe")
        elif linker == "NSLink":
            env["NSC_LLD_LINK"] = self.plan.nslink_exe
        if self.run_dir is not None:
            linker_name = safe_name(linker or "build")
            env["NSC_LOG_DIR"] = str(self.run_dir / linker_name)
            env["NSC_TEST_EVIDENCE_DIR"] = str(self.run_dir / linker_name / "evidence")
            if self.plan.keep_link_temps and linker is not None:
                env["NSC_KEEP_LINK_TEMPS"] = str(self.run_dir / linker_name / "linktemps")
        return env

    def _record_stage(
        self,
        name: str,
        status: str,
        seconds: float,
        exit_code: int,
        command: str,
        log: Path,
        report_failure: bool = True,
    ) -> None:
        row = {
            "name": name,
            "status": status,
            "seconds": round(seconds, 3),
            "exit_code": exit_code,
            "command": command,
            "log": str(log.relative_to(self.run_dir)) if self.run_dir else str(log),
        }
        self.stage_rows.append(row)
        self.metadata["stages"] = self.stage_rows
        self._save_metadata()
        self.stage_finished.emit(name, status, seconds, exit_code)
        if status == "failed":
            self.failed += 1
            if report_failure:
                linker_match = re.search(r"\[([^]]+)\]$", name)
                self._report_failure(
                    {
                        "linker": linker_match.group(1) if linker_match else "—",
                        "optimization": "General",
                        "source": name,
                        "detail": f"La etapa terminó con código {exit_code}",
                        "error_log": str(log.resolve()),
                    }
                )
        self._advance()

    def _command_stage(
        self,
        name: str,
        command: list[str],
        env: dict[str, str],
        timeout: int | None = None,
    ) -> bool:
        assert self.run_dir is not None
        self.stage_started.emit(name)
        log = self.run_dir / "stages" / f"{safe_name(name)}.log"
        result = self._run_process(command, Path(self.plan.root), env, log, timeout=timeout)
        if result.cancelled:
            status = "cancelled"
        else:
            status = "passed" if result.exit_code == 0 else "failed"
        self._record_stage(name, status, result.seconds, result.exit_code, command_text(command), log)
        return status == "passed"

    def _advance(self, count: int = 1) -> None:
        self.completed_units += count
        self.progress.emit(self.completed_units, self.total_units)

    def _validate_paths(self) -> list[str]:
        required: list[Path] = []
        building = self.plan.run_build and self.plan.build_mode != "skip"
        running_tests = self.plan.run_ctest or self.plan.run_cases or self.plan.run_triage
        if building:
            required.extend(
                [
                    Path(self.plan.llvm_root) / "bin" / "clang-cl.exe",
                    Path(self.plan.llvm_root) / "bin" / "llvm-mt.exe",
                    Path(self.plan.llvm_root) / "bin" / "llvm-rc.exe",
                    Path(self.plan.llvm_root) / "bin" / "lld-link.exe",
                    Path(self.plan.llvm_root) / "lib" / "LLVM-C.lib",
                ]
            )
            if shutil.which("cmake") is None:
                required.append(Path("cmake.exe (PATH)"))
            if self.plan.build_runner == "ninja" and shutil.which("ninja") is None:
                required.append(Path("ninja.exe (PATH)"))
        if running_tests:
            if not building:
                required.append(Path(self.plan.prefix) / "bin" / "nsc.exe")
            if "lld-link" in self.plan.linkers:
                required.append(Path(self.plan.llvm_root) / "bin" / "lld-link.exe")
            if "NSLink" in self.plan.linkers:
                required.append(Path(self.plan.nslink_exe))
        return [str(path) for path in required if not path.is_file()]

    def _configure_command(self) -> list[str]:
        llvm = Path(self.plan.llvm_root).as_posix()
        return [
            "cmake",
            "-S",
            self.plan.root,
            "-B",
            self.plan.build_dir,
            "-G",
            "Ninja",
            "-DCMAKE_BUILD_TYPE=Release",
            f"-DCMAKE_CXX_COMPILER={llvm}/bin/clang-cl.exe",
            f"-DCMAKE_LINKER={llvm}/bin/lld-link.exe",
            f"-DCMAKE_MT={llvm}/bin/llvm-mt.exe",
            f"-DCMAKE_RC_COMPILER={llvm}/bin/llvm-rc.exe",
            "-DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld",
            "-DCMAKE_DISABLE_FIND_PACKAGE_LLVM=TRUE",
            f"-DNSC_LLVM_MONOLITHIC_LIBRARY={llvm}/lib/LLVM-C.lib",
            "-DNSC_BUILD_TESTS=ON",
            "-DNSC_WARNINGS_AS_ERRORS=ON",
        ]

    def _build(self) -> bool:
        if not self.plan.run_build or self.plan.build_mode == "skip":
            return True
        root = Path(self.plan.root).resolve()
        build = Path(self.plan.build_dir).resolve()
        if self.plan.build_mode == "clean":
            if build.parent != root or not build.name.lower().startswith("build-"):
                self._emit(f"Se rechazó borrar un build fuera del destino permitido: {build}", "error")
                self.failed += 1
                return False
            self.stage_started.emit("Limpiar build")
            started = time.monotonic()
            try:
                if build.exists():
                    shutil.rmtree(build)
                status, code = "passed", 0
                self._emit(f"Build limpio: {build}")
            except OSError as exc:
                status, code = "failed", -1
                self._emit(f"No se pudo limpiar {build}: {exc!r}", "error")
            assert self.run_dir is not None
            log = self.run_dir / "stages" / "limpiar_build.log"
            log.parent.mkdir(parents=True, exist_ok=True)
            log.write_text(f"{status}: {build}\n", encoding="utf-8")
            self._record_stage("Limpiar build", status, time.monotonic() - started, code, "", log)
            if status != "passed":
                return False

        cache = build / "CMakeCache.txt"
        if self.plan.build_mode == "clean" or not cache.is_file():
            if not self._command_stage("Configurar", self._configure_command(), self._base_environment()):
                return False

        targets = [] if self.plan.build_target == "all" else [
            target for target in dict.fromkeys(self.plan.build_target.split(";")) if target
        ]
        if self.plan.build_runner == "ninja":
            command = [
                "ninja",
                "-C",
                self.plan.build_dir,
                "-j",
                str(self.plan.jobs),
                "-k",
                "0",
                *targets,
            ]
        else:
            command = ["cmake", "--build", self.plan.build_dir]
            if targets:
                command.extend(["--target", *targets])
            command.extend(["--parallel", str(self.plan.jobs), "--", "-k", "0"])
        if not self._command_stage("Construir", command, self._base_environment()):
            return False
        return self._command_stage(
            "Instalar",
            ["cmake", "--install", self.plan.build_dir, "--prefix", self.plan.prefix],
            self._base_environment(),
        )

    def _sync_libraries(self) -> bool:
        if not self.plan.sync_libraries:
            return True
        assert self.run_dir is not None
        name = "Sincronizar bibliotecas"
        self.stage_started.emit(name)
        started = time.monotonic()
        log = self.run_dir / "stages" / "sincronizar_bibliotecas.log"
        source_root = Path(self.plan.prefix).resolve() / "lib"
        destination_root = Path(self.plan.root).resolve() / "lib"
        libraries = ("NativeSharp.Core", "NativeSharp.Core.Collections.Generic")
        lines = [f"origen={source_root}", f"destino={destination_root}"]
        status = "passed"
        code = 0
        try:
            destination_root.mkdir(parents=True, exist_ok=True)
            sources: dict[str, Path] = {}
            for library in libraries:
                source = source_root / library
                destination = destination_root / library
                project = source / f"{library}.nsproj"
                if not project.is_file():
                    raise FileNotFoundError(f"Falta la biblioteca instalada: {project}")
                if source.resolve() == destination.resolve():
                    raise ValueError(f"El origen y el destino son el mismo directorio: {source}")
                sources[library] = source
            for library in libraries:
                source = sources[library]
                destination = destination_root / library
                if destination.exists():
                    shutil.rmtree(destination)
                shutil.copytree(source, destination)
                lines.append(f"copiada={library}")
                self._emit(f"[LIB] Copiada {library} desde {source}")
            (destination_root / ".source.txt").write_text(
                f"Synced from {source_root}\n", encoding="utf-8"
            )
        except (OSError, ValueError) as exc:
            status = "failed"
            code = 1
            lines.append(f"error={exc}")
            self._emit(f"[LIB] {exc}", "error")
        elapsed = time.monotonic() - started
        log.parent.mkdir(parents=True, exist_ok=True)
        log.write_text("\n".join(lines) + "\n", encoding="utf-8", errors="replace")
        self._record_stage(name, status, elapsed, code, "sincronización interna", log)
        return status == "passed"

    def _ctest(self, linker: str) -> None:
        if not self.plan.run_ctest or self.cancel_event.is_set():
            return
        assert self.run_dir is not None
        name = f"CTest [{linker}]"
        command = [
            "ctest",
            "--test-dir",
            self.plan.build_dir,
            "--output-on-failure",
            "-j",
            str(self.plan.jobs),
        ]
        if self.plan.selected_ctests:
            expression = "^(?:" + "|".join(re.escape(name) for name in self.plan.selected_ctests) + ")$"
            command.extend(["-R", expression])
        self.stage_started.emit(name)
        log = self.run_dir / "stages" / f"{safe_name(name)}.log"
        result = self._run_process(
            command,
            Path(self.plan.root),
            self._base_environment(linker),
            log,
            channel=linker,
        )
        status = "cancelled" if result.cancelled else (
            "passed" if result.exit_code == 0 else "failed"
        )
        self._record_stage(
            name,
            status,
            result.seconds,
            result.exit_code,
            command_text(command),
            log,
            report_failure=False,
        )
        if status != "failed":
            return
        failed_names = re.findall(
            r"^\s*\d+\s+-\s+([^\s]+)\s+\([^)]+\)\s*$", result.output, re.MULTILINE
        )
        if not failed_names:
            failed_names = re.findall(
                r"Test\s+#\d+:\s+([^\s]+).*?\*{3}(?:Failed|Timeout)", result.output
            )
        if not failed_names and len(self.plan.selected_ctests) == 1:
            failed_names = list(self.plan.selected_ctests)
        for failed_name in dict.fromkeys(failed_names or [name]):
            self._report_failure(
                {
                    "linker": linker,
                    "optimization": "General",
                    "source": failed_name,
                    "detail": f"CTest terminó con código {result.exit_code}",
                    "error_log": str(log.resolve()),
                }
            )

    def _case_commands(self, case: CaseSpec, exe: Path, optimization: str) -> tuple[list[str], list[str]]:
        project = Path(self.plan.root) / case.project
        compiler = Path(self.plan.prefix) / "bin" / "nsc.exe"
        obj = exe.with_suffix(".obj")
        common = [str(project), optimization]
        return (
            [str(compiler), "--emit-project-oop-obj", *common, "-o", str(obj)],
            [str(compiler), "--emit-project-oop-exe", *common, "-o", str(exe)],
        )

    def _run_one_case(self, case: CaseSpec, optimization: str, linker: str) -> dict:
        assert self.run_dir is not None
        key = f"{case.label}_{optimization.lstrip('-')}"
        pass_dir = self.run_dir / safe_name(linker)
        log = pass_dir / "cases" / f"{safe_name(key)}.log"
        exe = pass_dir / "bin" / f"{safe_name(key)}.exe"
        exe.parent.mkdir(parents=True, exist_ok=True)
        env = self._base_environment(linker)
        env["NSC_OPT"] = optimization
        env["NS_LIB_READY"] = "1"
        env["NSC_CC1_LINKER"] = env.get("NSC_LLD_LINK", "")
        prefix = f"[{linker} {case.label} {optimization}]"
        started = time.monotonic()
        actual: int | None = None
        error_log = log
        try:
            project = Path(self.plan.root) / case.project
            if not project.is_file():
                raise FileNotFoundError(f"Proyecto no encontrado: {project}")
            object_command, executable_command = self._case_commands(case, exe, optimization)
            object_build = self._run_process(
                object_command,
                Path(self.plan.root),
                env,
                log.with_suffix(".obj.log"),
                timeout=CASE_TIMEOUT_SECONDS,
                prefix=prefix,
                channel=linker,
                optimization=optimization.lstrip("-"),
            )
            if object_build.cancelled:
                status, actual, detail = "cancelled", None, "cancelado"
            elif object_build.exit_code != 0:
                status, detail = "failed", object_build.error or "falló la emisión del objeto"
                error_log = log.with_suffix(".obj.log")
            else:
                executable_build = self._run_process(
                    executable_command,
                    Path(self.plan.root),
                    env,
                    log.with_suffix(".exe.log"),
                    timeout=CASE_TIMEOUT_SECONDS,
                    prefix=prefix,
                    channel=linker,
                    optimization=optimization.lstrip("-"),
                )
                if executable_build.cancelled:
                    status, detail = "cancelled", "cancelado"
                elif executable_build.exit_code != 0:
                    status, detail = (
                        "failed",
                        executable_build.error or "falló la emisión o el enlace del ejecutable",
                    )
                    error_log = log.with_suffix(".exe.log")
                elif case.expected is None:
                    status, detail = "passed", "compila y enlaza"
                else:
                    run = self._run_process(
                        [str(exe)],
                        Path(self.plan.root),
                        env,
                        log.with_suffix(".run.log"),
                        timeout=CASE_TIMEOUT_SECONDS,
                        prefix=prefix,
                        channel=linker,
                        optimization=optimization.lstrip("-"),
                    )
                    actual = run.exit_code
                    if run.cancelled:
                        status, detail = "cancelled", "cancelado"
                    elif run.timed_out:
                        status, detail = "failed", run.error
                        error_log = log.with_suffix(".run.log")
                    elif actual == case.expected:
                        status, detail = "passed", f"exit {actual}"
                    else:
                        status, detail = "failed", f"exit {actual}; se esperaba {case.expected}"
                        error_log = log.with_suffix(".run.log")
            elapsed = time.monotonic() - started
            summary = [
                f"case={case.label}",
                f"project={case.project}",
                f"linker={linker}",
                f"optimization={optimization}",
                f"expected={case.expected}",
                f"actual={actual}",
                f"status={status}",
                f"seconds={elapsed:.3f}",
                f"detail={detail}",
            ]
            log.parent.mkdir(parents=True, exist_ok=True)
            log.write_text("\n".join(summary) + "\n", encoding="utf-8", errors="replace")
            return {
                "label": case.label,
                "project": case.project,
                "kind": "triage" if case.source == "bug triage" else "fixture",
                "linker": linker,
                "optimization": optimization,
                "expected": case.expected,
                "actual": actual,
                "status": status,
                "detail": detail,
                "seconds": round(elapsed, 3),
                "log": str(log.relative_to(self.run_dir)),
                "error_log": str(error_log.resolve()),
            }
        except BaseException as exc:  # cada worker SIEMPRE produce un resultado y un log
            elapsed = time.monotonic() - started
            detail = f"excepción del runner: {exc!r}"
            log.parent.mkdir(parents=True, exist_ok=True)
            log.write_text(traceback.format_exc(), encoding="utf-8", errors="replace")
            self._emit(f"{prefix} {detail}", "error", linker, optimization.lstrip("-"))
            return {
                "label": case.label,
                "project": case.project,
                "kind": "triage" if case.source == "bug triage" else "fixture",
                "linker": linker,
                "optimization": optimization,
                "expected": case.expected,
                "actual": None,
                "status": "failed",
                "detail": detail,
                "seconds": round(elapsed, 3),
                "log": str(log.relative_to(self.run_dir)),
                "error_log": str(log.resolve()),
            }

    def _run_case_group(self, linker: str) -> None:
        if not self.plan.run_cases or self.cancel_event.is_set():
            return
        selected = set(self.plan.selected_cases)
        cases = [case for case in self.cases if not selected or case.label in selected]
        work = [(case, opt) for case in cases for opt in self.plan.optimizations]
        name = f"Fixtures [{linker}]"
        self.stage_started.emit(name)
        started = time.monotonic()
        before_failed = self.failed
        self._emit(f"{name}: {len(work)} trabajos, {self.plan.jobs} en paralelo", "info", linker)
        with ThreadPoolExecutor(max_workers=self.plan.jobs) as pool:
            futures = {
                pool.submit(self._run_one_case, case, optimization, linker): (case, optimization)
                for case, optimization in work
            }
            for future in as_completed(futures):
                row = future.result()
                self.case_rows.append(row)
                if row["status"] == "failed":
                    self.failed += 1
                    level = "error"
                    self._report_failure(
                        {
                            "linker": row["linker"],
                            "optimization": row["optimization"].lstrip("-"),
                            "source": f"{row['label']} {row['optimization']}",
                            "detail": row["detail"],
                            "error_log": row["error_log"],
                        }
                    )
                elif row["status"] == "cancelled":
                    level = "warn"
                else:
                    level = "success"
                self._emit(
                    f"[{linker}] {row['label']} {row['optimization']}: "
                    f"{row['status'].upper()} — {row['detail']} ({row['seconds']:.2f} s)",
                    level,
                    linker,
                    row["optimization"].lstrip("-"),
                )
                self.metadata["cases"] = self.case_rows
                self._save_metadata()
                self._advance()
        elapsed = time.monotonic() - started
        status = "cancelled" if self.cancel_event.is_set() else (
            "failed" if self.failed > before_failed else "passed"
        )
        assert self.run_dir is not None
        log = self.run_dir / "stages" / f"{safe_name(name)}.log"
        group_rows = [
            row
            for row in self.case_rows
            if row["linker"] == linker and row.get("kind") == "fixture"
        ]
        log.parent.mkdir(parents=True, exist_ok=True)
        log.write_text(json.dumps(group_rows, indent=2, ensure_ascii=False), encoding="utf-8")
        # Los fallos de casos ya se contaron individualmente; la etapa es sólo un resumen.
        stage_row = {
            "name": name,
            "status": status,
            "seconds": round(elapsed, 3),
            "exit_code": 0 if status == "passed" else 1,
            "command": "runner interno",
            "log": str(log.relative_to(self.run_dir)),
        }
        self.stage_rows.append(stage_row)
        self.metadata["stages"] = self.stage_rows
        self._save_metadata()
        self.stage_finished.emit(name, status, elapsed, stage_row["exit_code"])

    def _triage(self, linker: str) -> None:
        if not self.plan.run_triage or self.cancel_event.is_set():
            return
        name = f"Bug triage [{linker}]"
        self.stage_started.emit(name)
        started = time.monotonic()
        before_failed = self.failed
        cases = [CaseSpec(project, label, 42, "bug triage") for project, label in TRIAGE_CASES]
        work = [(case, optimization) for case in cases for optimization in ("-O0", "-O2")]
        self._emit(
            f"{name}: {len(work)} trabajos internos, {self.plan.jobs} en paralelo",
            "info",
            linker,
        )
        with ThreadPoolExecutor(max_workers=self.plan.jobs) as pool:
            futures = {
                pool.submit(self._run_one_case, case, optimization, linker): (case, optimization)
                for case, optimization in work
            }
            for future in as_completed(futures):
                row = future.result()
                self.case_rows.append(row)
                if row["status"] == "failed":
                    self.failed += 1
                    level = "error"
                    self._report_failure(
                        {
                            "linker": row["linker"],
                            "optimization": row["optimization"].lstrip("-"),
                            "source": f"{row['label']} {row['optimization']}",
                            "detail": row["detail"],
                            "error_log": row["error_log"],
                        }
                    )
                elif row["status"] == "cancelled":
                    level = "warn"
                else:
                    level = "success"
                self._emit(
                    f"[{linker} triage] {row['label']} {row['optimization']}: "
                    f"{row['status'].upper()} — {row['detail']} ({row['seconds']:.2f} s)",
                    level,
                    linker,
                    row["optimization"].lstrip("-"),
                )
                self.metadata["cases"] = self.case_rows
                self._save_metadata()
                self._advance()
        elapsed = time.monotonic() - started
        status = "cancelled" if self.cancel_event.is_set() else (
            "failed" if self.failed > before_failed else "passed"
        )
        assert self.run_dir is not None
        log = self.run_dir / "stages" / f"{safe_name(name)}.log"
        triage_rows = [
            row
            for row in self.case_rows
            if row["linker"] == linker and row.get("kind") == "triage"
        ]
        log.parent.mkdir(parents=True, exist_ok=True)
        log.write_text(json.dumps(triage_rows, indent=2, ensure_ascii=False), encoding="utf-8")
        stage_row = {
            "name": name,
            "status": status,
            "seconds": round(elapsed, 3),
            "exit_code": 0 if status == "passed" else 1,
            "command": "runner interno",
            "log": str(log.relative_to(self.run_dir)),
        }
        self.stage_rows.append(stage_row)
        self.metadata["stages"] = self.stage_rows
        self._save_metadata()
        self.stage_finished.emit(name, status, elapsed, stage_row["exit_code"])

    def _calculate_total(self) -> int:
        total = 0
        if self.plan.run_build and self.plan.build_mode != "skip":
            if self.plan.build_mode == "clean":
                total += 4  # limpiar, configurar, construir, instalar
            elif not (Path(self.plan.build_dir) / "CMakeCache.txt").is_file():
                total += 3  # configurar, construir, instalar
            else:
                total += 2  # construir, instalar
        if self.plan.sync_libraries:
            total += 1
        selected = set(self.plan.selected_cases)
        case_count = len([case for case in self.cases if not selected or case.label in selected])
        for _ in self.plan.linkers:
            total += int(self.plan.run_ctest)
            if self.plan.run_triage:
                total += len(TRIAGE_CASES) * 2
            if self.plan.run_cases:
                total += case_count * len(self.plan.optimizations)
        return max(total, 1)

    def _write_error_reports(self) -> list[dict]:
        assert self.run_dir is not None
        reports: list[dict] = []
        linkers = list(dict.fromkeys(["General", *self.plan.linkers]))
        optimizations = ("General", "O0", "O2")
        for linker in linkers:
            for optimization in optimizations:
                rows = [
                    row
                    for row in self.failure_rows
                    if (row.get("linker") if row.get("linker") in self.plan.linkers else "General")
                    == linker
                    and str(row.get("optimization", "General")).lstrip("-") == optimization
                ]
                report = (
                    self.run_dir
                    / "errors"
                    / safe_name(linker)
                    / f"{safe_name(optimization)}.md"
                )
                lines = [
                    f"# Errores — {linker} — {optimization}",
                    "",
                    f"- Corrida: `{self.metadata.get('id', '')}`",
                    f"- Enlazador: `{linker}`",
                    f"- Optimización: `{optimization}`",
                    f"- Errores: {len(rows)}",
                    "",
                ]
                if not rows:
                    lines.append("No se encontraron errores en este canal.")
                for index, row in enumerate(rows, 1):
                    log_path = Path(str(row.get("error_log", "")))
                    lines.extend(
                        [
                            f"## {index}. {row.get('source', 'Error')}",
                            "",
                            f"- Causa: {row.get('detail', '')}",
                            f"- Log: `{log_path}`",
                            "",
                        ]
                    )
                    if log_path.is_file():
                        detail = log_path.read_text(encoding="utf-8", errors="replace")
                        if len(detail) > 20000:
                            detail = "[... contenido anterior omitido ...]\n" + detail[-20000:]
                        lines.extend(["```text", detail.rstrip(), "```", ""])
                report.parent.mkdir(parents=True, exist_ok=True)
                report.write_text("\n".join(lines) + "\n", encoding="utf-8")
                reports.append(
                    {
                        "linker": linker,
                        "optimization": optimization,
                        "errors": len(rows),
                        "report": str(report.relative_to(self.run_dir)),
                    }
                )
        return reports

    def _write_report(self) -> None:
        assert self.run_dir is not None
        status = self.metadata.get("status", "unknown")
        error_reports = self._write_error_reports()
        lines = [
            "# NSC Builder — reporte de ejecución",
            "",
            f"- Inicio: {self.metadata.get('started_at', '')}",
            f"- Fin: {self.metadata.get('finished_at', '')}",
            f"- Estado: **{status}**",
            f"- Tiempo total: {self.metadata.get('total_seconds', 0):.3f} s",
            f"- Fallos: {self.metadata.get('failed', 0)}",
            "",
            "## Etapas",
            "",
            "| Etapa | Estado | Tiempo | Exit | Log |",
            "|---|---:|---:|---:|---|",
        ]
        for row in self.stage_rows:
            lines.append(
                f"| {row['name']} | {row['status']} | {row['seconds']:.3f} s | "
                f"{row['exit_code']} | `{row['log']}` |"
            )
        lines.extend(
            [
                "",
                "## Errores por enlazador y optimización",
                "",
                "| Enlazador | Optimización | Errores | Informe detallado |",
                "|---|---:|---:|---|",
            ]
        )
        for error_report in error_reports:
            lines.append(
                f"| {error_report['linker']} | {error_report['optimization']} | "
                f"{error_report['errors']} | `{error_report['report']}` |"
            )
        if self.case_rows:
            lines.extend(
                [
                    "",
                    "## Casos ejecutados",
                    "",
                    "| Tipo | Enlazador | Caso | Opt | Estado | Resultado | Tiempo | Log |",
                    "|---|---|---|---:|---:|---|---:|---|",
                ]
            )
            for row in sorted(
                self.case_rows, key=lambda item: (item["linker"], item["label"], item["optimization"])
            ):
                lines.append(
                    f"| {row.get('kind', 'fixture')} | {row['linker']} | "
                    f"{row['label']} | {row['optimization']} | "
                    f"{row['status']} | {row['detail']} | {row['seconds']:.3f} s | "
                    f"`{row['log']}` |"
                )
        (self.run_dir / "report.md").write_text("\n".join(lines) + "\n", encoding="utf-8")

    @Slot()
    def run(self) -> None:
        started = time.monotonic()
        run_name = datetime.now().strftime("%Y%m%d-%H%M%S") + "-" + uuid.uuid4().hex[:8]
        self.run_dir = HISTORY_ROOT / run_name
        self.run_dir.mkdir(parents=True, exist_ok=False)
        self.console_path = self.run_dir / "console.log"
        self.console_path.touch()
        for channel in dict.fromkeys(["General", *self.plan.linkers]):
            for optimization in ("General", "O0", "O2"):
                channel_console = (
                    self.run_dir
                    / safe_name(channel)
                    / safe_name(optimization)
                    / "console.log"
                )
                channel_console.parent.mkdir(parents=True, exist_ok=True)
                channel_console.touch()
        self.total_units = self._calculate_total()
        self.metadata = {
            "id": run_name,
            "started_at": utc_stamp(),
            "finished_at": None,
            "status": "running",
            "total_seconds": 0.0,
            "failed": 0,
            "plan": asdict(self.plan),
            "stages": [],
            "cases": [],
            "failures": [],
        }
        self._save_metadata()
        self.progress.emit(0, self.total_units)
        self._emit(f"Resultados: {self.run_dir}", "success")
        try:
            missing = self._validate_paths()
            if missing:
                for path in missing:
                    self._emit(f"Falta herramienta: {path}", "error")
                    self._report_failure(
                        {
                            "linker": "General",
                            "optimization": "General",
                            "source": "Validación de herramientas",
                            "detail": f"Falta herramienta: {path}",
                            "error_log": str(self.console_path.resolve()),
                        }
                    )
                self.failed += len(missing)
            elif self._build() and not self.cancel_event.is_set():
                if self._sync_libraries() and not self.cancel_event.is_set():
                    for linker in self.plan.linkers:
                        self._ctest(linker)
                        self._run_case_group(linker)
                        self._triage(linker)
                        if self.cancel_event.is_set():
                            break
        except BaseException as exc:
            self.failed += 1
            self._emit(f"Fallo interno del runner: {exc!r}", "error")
            exception_log = self.run_dir / "runner_exception.log"
            exception_log.write_text(
                traceback.format_exc(), encoding="utf-8", errors="replace"
            )
            self._report_failure(
                {
                    "linker": "General",
                    "optimization": "General",
                    "source": "Runner interno",
                    "detail": repr(exc),
                    "error_log": str(exception_log.resolve()),
                }
            )
        finally:
            elapsed = time.monotonic() - started
            if self.cancel_event.is_set():
                status = "cancelled"
            else:
                status = "passed" if self.failed == 0 else "failed"
            self.metadata.update(
                {
                    "finished_at": utc_stamp(),
                    "status": status,
                    "total_seconds": round(elapsed, 3),
                    "failed": self.failed,
                    "stages": self.stage_rows,
                    "cases": self.case_rows,
                    "failures": self.failure_rows,
                }
            )
            self._save_metadata()
            self._write_report()
            self._emit(f"Corrida {status.upper()} en {elapsed:.2f} s", "success" if status == "passed" else "error")
            self.finished.emit(
                {
                    "status": status,
                    "seconds": elapsed,
                    "failed": self.failed,
                    "run_dir": str(self.run_dir),
                    "report": str(self.run_dir / "report.md"),
                }
            )


class MainWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle(APP_NAME)
        self.resize(1420, 900)
        self.settings = QSettings("NativeSharp", "NSCBuilder")
        self.cases = import_case_table(ROOT)
        self.case_groups: dict[str, list[str]] = {}
        self.ctest_groups: dict[str, list[str]] = {}
        self.ctest_targets: dict[str, str] = {}
        self.thread: QThread | None = None
        self.worker: PipelineWorker | None = None
        self.inventory_process: QProcess | None = None
        self.inventory_bytes = bytearray()
        self.current_run_dir: Path | None = None
        self.stage_row_by_name: dict[str, int] = {}
        self.close_after_cancel = False
        self.applying_run_mode = False
        self._build_ui()
        self._load_settings()
        self._populate_cases()
        QTimer.singleShot(0, self.refresh_ctests)

    def _build_ui(self) -> None:
        central = QWidget()
        central.setMaximumSize(0, 0)
        self.setCentralWidget(central)
        self.setDockNestingEnabled(True)
        config_panel = QWidget()
        config_layout = QVBoxLayout(config_panel)

        options = QGroupBox("Ejecución")
        grid = QGridLayout(options)
        self.run_mode = QComboBox()
        self.run_mode.addItem("Construir y ejecutar pruebas", "build_and_tests")
        self.run_mode.addItem("Sólo pruebas — no reconstruir ni instalar nsc", "tests_only")
        self.run_mode.addItem("Sólo construir e instalar nsc", "build_only")
        self.run_mode.addItem("Personalizado", "custom")
        self.build_mode = QComboBox()
        self.build_mode.addItem("Desde cero", "clean")
        self.build_mode.addItem("Incremental", "incremental")
        self.build_mode.addItem("Omitir build", "skip")
        self.build_target = QComboBox()
        self.build_target.addItem("Automático según las pruebas elegidas", "auto")
        self.build_target.addItem("Compilador y todas las pruebas", "all")
        self.build_target.addItem("Sólo el compilador nsc", "nsc")
        self.build_runner = QComboBox()
        self.build_runner.addItem("CMake (cmake --build)", "cmake")
        self.build_runner.addItem("Ninja directo", "ninja")
        self.jobs = QSpinBox()
        self.jobs.setRange(1, 128)
        self.jobs.setValue(min(os.cpu_count() or 4, 16))
        self.opt_o0 = QCheckBox("O0")
        self.opt_o0.setChecked(True)
        self.opt_o2 = QCheckBox("O2")
        self.opt_o2.setChecked(True)
        opt_box = QWidget()
        opt_layout = QHBoxLayout(opt_box)
        opt_layout.setContentsMargins(0, 0, 0, 0)
        opt_layout.addWidget(self.opt_o0)
        opt_layout.addWidget(self.opt_o2)
        opt_layout.addStretch()

        grid.addWidget(QLabel("Modo"), 0, 0)
        grid.addWidget(self.run_mode, 0, 1, 1, 7)
        grid.addWidget(QLabel("Build"), 1, 0)
        grid.addWidget(self.build_mode, 1, 1)
        grid.addWidget(QLabel("Objetivos"), 1, 2)
        grid.addWidget(self.build_target, 1, 3)
        grid.addWidget(QLabel("Paralelismo (build y pruebas)"), 1, 4)
        grid.addWidget(self.jobs, 1, 5)
        grid.addWidget(QLabel("Optimización"), 1, 6)
        grid.addWidget(opt_box, 1, 7)

        self.stage_build = QCheckBox("Construir e instalar")
        self.stage_build.setChecked(True)
        self.stage_sync = QCheckBox("Sincronizar bibliotecas")
        self.stage_sync.setChecked(True)
        self.stage_ctest = QCheckBox("CTest")
        self.stage_ctest.setChecked(True)
        self.stage_cases = QCheckBox("Fixtures y ejemplos")
        self.stage_cases.setChecked(True)
        self.stage_triage = QCheckBox("Bug triage")
        self.stage_triage.setChecked(True)
        self.keep_temps = QCheckBox("Conservar temporales de enlace")
        stages = QWidget()
        stages_layout = QHBoxLayout(stages)
        stages_layout.setContentsMargins(0, 0, 0, 0)
        for widget in (
            self.stage_build,
            self.stage_sync,
            self.stage_ctest,
            self.stage_cases,
            self.stage_triage,
            self.keep_temps,
        ):
            stages_layout.addWidget(widget)
        stages_layout.addStretch()
        grid.addWidget(QLabel("Motor de construcción"), 2, 0)
        grid.addWidget(self.build_runner, 2, 1, 1, 3)
        runner_note = QLabel("CMake siempre configura e instala; aquí eliges cmake --build o Ninja para compilar.")
        runner_note.setWordWrap(True)
        runner_note.setObjectName("muted")
        grid.addWidget(runner_note, 2, 4, 1, 4)
        grid.addWidget(stages, 3, 0, 1, 8)
        build_note = QLabel(
            "La construcción del compilador usa siempre clang-cl + lld-link. "
            "El enlazador NSLink sólo puede seleccionarse para los programas de prueba."
        )
        build_note.setWordWrap(True)
        build_note.setObjectName("muted")
        grid.addWidget(build_note, 4, 0, 1, 8)
        config_layout.addWidget(options)

        paths = QGroupBox("Rutas")
        form = QFormLayout(paths)
        self.build_path = QLineEdit(str(DEFAULT_BUILD))
        self.prefix_path = QLineEdit(str(DEFAULT_PREFIX))
        self.llvm_path = QLineEdit(str(DEFAULT_LLVM))
        self.nslink_path = QLineEdit(str(DEFAULT_NSLINK))
        form.addRow("Build", self.build_path)
        form.addRow("Instalación", self.prefix_path)
        form.addRow("LLVM", self.llvm_path)
        form.addRow("NSLink", self.nslink_path)
        config_layout.addWidget(paths)
        config_layout.addStretch()
        self.config_dock = self._create_dock(
            "Construcción", config_panel, Qt.DockWidgetArea.LeftDockWidgetArea, "dock_build"
        )

        selection = QWidget()
        self.test_panel = selection
        selection_layout = QVBoxLayout(selection)
        test_options = QGroupBox("Configuración de pruebas")
        test_grid = QGridLayout(test_options)
        self.linker_mode = QComboBox()
        self.linker_mode.addItem("lld-link", ["lld-link"])
        self.linker_mode.addItem("NSLink", ["NSLink"])
        self.linker_mode.addItem("Ambos (dos pasadas)", ["lld-link", "NSLink"])
        test_grid.addWidget(QLabel("Enlazador usado por nsc para los programas de prueba"), 0, 0)
        test_grid.addWidget(self.linker_mode, 0, 1)
        linker_note = QLabel(
            "Esto no cambia cómo se construye nsc.exe; sólo fija el enlazador que usará nsc "
            "cuando CTest, los fixtures o bug triage le pidan producir un ejecutable."
        )
        linker_note.setWordWrap(True)
        linker_note.setObjectName("muted")
        test_grid.addWidget(linker_note, 1, 0, 1, 2)
        selection_layout.addWidget(test_options)
        selection_tabs = QTabWidget()
        selection_tabs.addTab(self._ctest_page(), "CTest")
        selection_tabs.addTab(self._case_page(), "Fixtures")
        selection_layout.addWidget(selection_tabs, 1)
        self.tests_dock = self._create_dock(
            "Pruebas", selection, Qt.DockWidgetArea.LeftDockWidgetArea, "dock_tests"
        )
        self.splitDockWidget(self.config_dock, self.tests_dock, Qt.Orientation.Vertical)

        console_panel = QWidget()
        right_layout = QVBoxLayout(console_panel)
        controls = QHBoxLayout()
        self.start_button = QPushButton("Iniciar")
        self.start_button.clicked.connect(self.start_run)
        self.cancel_button = QPushButton("Cancelar")
        self.cancel_button.setEnabled(False)
        self.cancel_button.clicked.connect(self.cancel_run)
        self.open_button = QPushButton("Abrir resultados")
        self.open_button.setEnabled(False)
        self.open_button.clicked.connect(self.open_results)
        self.clear_button = QPushButton("Limpiar consola")
        self.clear_button.clicked.connect(self.clear_current_console)
        self.status_label = QLabel("Listo")
        controls.addWidget(self.start_button)
        controls.addWidget(self.cancel_button)
        controls.addWidget(self.open_button)
        controls.addWidget(self.clear_button)
        controls.addWidget(self.status_label, 1)
        right_layout.addLayout(controls)

        self.plan_summary = QLabel()
        self.plan_summary.setWordWrap(True)
        self.plan_summary.setObjectName("planSummary")
        right_layout.addWidget(self.plan_summary)

        self.progress_bar = QProgressBar()
        self.progress_bar.setRange(0, 1)
        self.progress_bar.setValue(0)
        right_layout.addWidget(self.progress_bar)

        self.output_channels = ("General", "lld-link", "NSLink")
        self.output_optimizations = ("General", "O0", "O2")
        self.channel_consoles: dict[tuple[str, str], QTextEdit] = {}
        self.channel_error_tables: dict[tuple[str, str], QTableWidget] = {}
        self.channel_inner_tabs: dict[tuple[str, str], QTabWidget] = {}
        self.channel_error_tab_indices: dict[tuple[str, str], int] = {}
        self.channel_outer_indices: dict[str, int] = {}
        self.channel_optimization_tabs: dict[str, QTabWidget] = {}
        self.channel_optimization_indices: dict[tuple[str, str], int] = {}
        self.output_channel_tabs = QTabWidget()
        for channel in self.output_channels:
            optimization_tabs = QTabWidget()
            for optimization in self.output_optimizations:
                key = (channel, optimization)
                console = self._create_console()
                error_table = self._create_error_table()
                inner_tabs = QTabWidget()
                inner_tabs.addTab(console, "Consola")
                error_index = inner_tabs.addTab(error_table, "Errores (0)")
                optimization_index = optimization_tabs.addTab(inner_tabs, optimization)
                self.channel_consoles[key] = console
                self.channel_error_tables[key] = error_table
                self.channel_inner_tabs[key] = inner_tabs
                self.channel_error_tab_indices[key] = error_index
                self.channel_optimization_indices[key] = optimization_index
            outer_index = self.output_channel_tabs.addTab(optimization_tabs, channel)
            self.channel_optimization_tabs[channel] = optimization_tabs
            self.channel_outer_indices[channel] = outer_index
        right_layout.addWidget(self.output_channel_tabs, 3)
        self.console_dock = self._create_dock(
            "Salida", console_panel, Qt.DockWidgetArea.RightDockWidgetArea, "dock_console"
        )

        self.stage_table = QTableWidget(0, 4)
        self.stage_table.setHorizontalHeaderLabels(["Etapa", "Estado", "Tiempo", "Exit"])
        self.stage_table.verticalHeader().setVisible(False)
        self.stage_table.horizontalHeader().setSectionResizeMode(0, QHeaderView.ResizeMode.Stretch)
        self.stage_table.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers)
        self.results_dock = self._create_dock(
            "Resultados de etapas",
            self.stage_table,
            Qt.DockWidgetArea.RightDockWidgetArea,
            "dock_results",
        )
        self.splitDockWidget(self.console_dock, self.results_dock, Qt.Orientation.Vertical)

        self.view_menu = self.menuBar().addMenu("Ver")
        for dock in (self.config_dock, self.tests_dock, self.console_dock, self.results_dock):
            self.view_menu.addAction(dock.toggleViewAction())

        self.run_mode.currentIndexChanged.connect(self._apply_run_mode)
        for widget in (
            self.build_mode,
            self.build_target,
            self.build_runner,
            self.linker_mode,
        ):
            widget.currentIndexChanged.connect(self._update_plan_summary)
        for widget in (
            self.stage_build,
            self.stage_sync,
            self.stage_ctest,
            self.stage_cases,
            self.stage_triage,
        ):
            widget.toggled.connect(self._stage_selection_changed)
        self._apply_run_mode()

    def _create_console(self) -> QTextEdit:
        console = QTextEdit()
        console.setReadOnly(True)
        console.setAcceptRichText(False)
        console.setLineWrapMode(QTextEdit.LineWrapMode.NoWrap)
        console.setFont(QFontDatabase.systemFont(QFontDatabase.SystemFont.FixedFont))
        return console

    def _create_error_table(self) -> QTableWidget:
        table = QTableWidget(0, 4)
        table.setHorizontalHeaderLabels(["Enlazador", "Prueba o etapa", "Error", "Log"])
        table.verticalHeader().setVisible(False)
        table.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        table.setSelectionMode(QAbstractItemView.SelectionMode.SingleSelection)
        table.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers)
        table.horizontalHeader().setSectionResizeMode(0, QHeaderView.ResizeMode.ResizeToContents)
        table.horizontalHeader().setSectionResizeMode(1, QHeaderView.ResizeMode.ResizeToContents)
        table.horizontalHeader().setSectionResizeMode(2, QHeaderView.ResizeMode.Stretch)
        table.horizontalHeader().setSectionResizeMode(3, QHeaderView.ResizeMode.ResizeToContents)
        table.itemDoubleClicked.connect(self.open_error_log)
        return table

    def _create_dock(
        self, title: str, widget: QWidget, area: Qt.DockWidgetArea, object_name: str
    ) -> QDockWidget:
        dock = QDockWidget(title, self)
        dock.setObjectName(object_name)
        dock.setAllowedAreas(Qt.DockWidgetArea.AllDockWidgetAreas)
        dock.setFeatures(
            QDockWidget.DockWidgetFeature.DockWidgetClosable
            | QDockWidget.DockWidgetFeature.DockWidgetMovable
            | QDockWidget.DockWidgetFeature.DockWidgetFloatable
        )
        dock.setWidget(widget)
        self.addDockWidget(area, dock)
        return dock

    def _ctest_page(self) -> QWidget:
        page = QWidget()
        layout = QVBoxLayout(page)
        scope_row = QGridLayout()
        self.ctest_scope = QComboBox()
        self.ctest_scope.addItem("Todas las pruebas CTest", "all")
        self.ctest_scope.addItem("Un grupo", "group")
        self.ctest_scope.addItem("Una sola prueba", "one")
        self.ctest_group_combo = QComboBox()
        self.ctest_one_combo = QComboBox()
        scope_row.addWidget(QLabel("Alcance"), 0, 0)
        scope_row.addWidget(self.ctest_scope, 0, 1)
        scope_row.addWidget(QLabel("Grupo"), 1, 0)
        scope_row.addWidget(self.ctest_group_combo, 1, 1)
        scope_row.addWidget(QLabel("Prueba"), 2, 0)
        scope_row.addWidget(self.ctest_one_combo, 2, 1)
        layout.addLayout(scope_row)
        row = QHBoxLayout()
        self.ctest_filter = QLineEdit()
        self.ctest_filter.setPlaceholderText("Filtrar nombres...")
        self.ctest_filter.textChanged.connect(lambda text: self._filter_list(self.ctest_list, text))
        refresh = QPushButton("Actualizar")
        refresh.clicked.connect(self.refresh_ctests)
        run_one = QPushButton("Sólo ejecutar esta prueba")
        run_one.clicked.connect(self._run_one_ctest_without_build)
        prepare = QPushButton("Construir nsc y ejecutar esta prueba")
        prepare.clicked.connect(self._run_one_ctest_with_build)
        row.addWidget(self.ctest_filter, 1)
        row.addWidget(refresh)
        row.addWidget(run_one)
        row.addWidget(prepare)
        layout.addLayout(row)
        self.ctest_scope_label = QLabel()
        self.ctest_scope_label.setWordWrap(True)
        layout.addWidget(self.ctest_scope_label)
        self.ctest_list = QListWidget()
        self.ctest_list.setSelectionMode(QAbstractItemView.SelectionMode.SingleSelection)
        self.ctest_list.currentItemChanged.connect(self._ctest_list_changed)
        layout.addWidget(self.ctest_list)
        self.ctest_scope.currentIndexChanged.connect(self._update_ctest_scope)
        self.ctest_group_combo.currentIndexChanged.connect(self._update_ctest_scope)
        self.ctest_one_combo.currentIndexChanged.connect(self._update_ctest_scope)
        return page

    def _case_page(self) -> QWidget:
        page = QWidget()
        layout = QVBoxLayout(page)
        scope_row = QGridLayout()
        self.case_scope = QComboBox()
        self.case_scope.addItem("Todos los fixtures y ejemplos", "all")
        self.case_scope.addItem("Un grupo", "group")
        self.case_scope.addItem("Un solo caso", "one")
        self.case_group_combo = QComboBox()
        self.case_one_combo = QComboBox()
        scope_row.addWidget(QLabel("Alcance"), 0, 0)
        scope_row.addWidget(self.case_scope, 0, 1)
        scope_row.addWidget(QLabel("Grupo"), 1, 0)
        scope_row.addWidget(self.case_group_combo, 1, 1)
        scope_row.addWidget(QLabel("Caso"), 2, 0)
        scope_row.addWidget(self.case_one_combo, 2, 1)
        layout.addLayout(scope_row)
        row = QHBoxLayout()
        self.case_filter = QLineEdit()
        self.case_filter.setPlaceholderText("Filtrar casos...")
        self.case_filter.textChanged.connect(lambda text: self._filter_list(self.case_list, text))
        run_one = QPushButton("Sólo ejecutar este caso")
        run_one.clicked.connect(self._run_one_case_without_build)
        prepare = QPushButton("Construir nsc y ejecutar este caso")
        prepare.clicked.connect(self._run_one_case_with_build)
        row.addWidget(self.case_filter, 1)
        row.addWidget(run_one)
        row.addWidget(prepare)
        layout.addLayout(row)
        self.case_scope_label = QLabel()
        self.case_scope_label.setWordWrap(True)
        layout.addWidget(self.case_scope_label)
        self.case_list = QListWidget()
        self.case_list.setSelectionMode(QAbstractItemView.SelectionMode.SingleSelection)
        self.case_list.currentItemChanged.connect(self._case_list_changed)
        layout.addWidget(self.case_list)
        self.case_scope.currentIndexChanged.connect(self._update_case_scope)
        self.case_group_combo.currentIndexChanged.connect(self._update_case_scope)
        self.case_one_combo.currentIndexChanged.connect(self._update_case_scope)
        return page

    @staticmethod
    def _filter_list(widget: QListWidget, text: str) -> None:
        needle = text.casefold().strip()
        for index in range(widget.count()):
            item = widget.item(index)
            item.setHidden(bool(needle) and needle not in item.text().casefold())

    def _populate_cases(self) -> None:
        self.case_list.clear()
        self.case_one_combo.clear()
        self.case_groups.clear()
        for case in sorted(self.cases, key=lambda item: item.label.casefold()):
            item = QListWidgetItem(case.label)
            item.setToolTip(f"{case.project}\nEsperado: {case.expected}\nFuente: {case.source}")
            item.setData(Qt.ItemDataRole.UserRole, case.label)
            self.case_list.addItem(item)
            self.case_one_combo.addItem(case.label, case.label)
            self.case_groups.setdefault(case_group(case), []).append(case.label)
        self.case_group_combo.clear()
        for group in sorted(self.case_groups, key=str.casefold):
            self.case_group_combo.addItem(f"{group} ({len(self.case_groups[group])})", group)
        self._update_case_scope()

    @staticmethod
    def _values_for_scope(
        scope: QComboBox,
        group_combo: QComboBox,
        one_combo: QComboBox,
        groups: dict[str, list[str]],
    ) -> list[str]:
        mode = str(scope.currentData())
        if mode == "all":
            return []
        if mode == "group":
            return list(groups.get(str(group_combo.currentData() or ""), []))
        value = one_combo.currentData()
        return [str(value)] if value else []

    def _selected_ctest_values(self) -> list[str]:
        return self._values_for_scope(
            self.ctest_scope,
            self.ctest_group_combo,
            self.ctest_one_combo,
            self.ctest_groups,
        )

    def _selected_case_values(self) -> list[str]:
        return self._values_for_scope(
            self.case_scope,
            self.case_group_combo,
            self.case_one_combo,
            self.case_groups,
        )

    @Slot()
    def _update_ctest_scope(self, *_args: object) -> None:
        mode = str(self.ctest_scope.currentData())
        self.ctest_group_combo.setEnabled(mode == "group")
        self.ctest_one_combo.setEnabled(mode == "one")
        if mode == "all":
            detail = "Se ejecutará todo CTest."
        else:
            count = len(self._selected_ctest_values())
            detail = f"Se ejecutarán exactamente {count} prueba(s) CTest."
        self.ctest_scope_label.setText(detail)
        self._update_plan_summary()

    @Slot()
    def _update_case_scope(self, *_args: object) -> None:
        mode = str(self.case_scope.currentData())
        self.case_group_combo.setEnabled(mode == "group")
        self.case_one_combo.setEnabled(mode == "one")
        if mode == "all":
            detail = f"Se ejecutarán los {len(self.cases)} fixtures y ejemplos descubiertos."
        else:
            count = len(self._selected_case_values())
            detail = f"Se ejecutarán exactamente {count} caso(s)."
        self.case_scope_label.setText(detail)
        self._update_plan_summary()

    @Slot(QListWidgetItem, QListWidgetItem)
    def _ctest_list_changed(
        self, current: QListWidgetItem | None, _previous: QListWidgetItem | None
    ) -> None:
        if current is None:
            return
        index = self.ctest_one_combo.findData(current.text())
        if index >= 0:
            self.ctest_one_combo.setCurrentIndex(index)

    @Slot(QListWidgetItem, QListWidgetItem)
    def _case_list_changed(
        self, current: QListWidgetItem | None, _previous: QListWidgetItem | None
    ) -> None:
        if current is None:
            return
        value = current.data(Qt.ItemDataRole.UserRole) or current.text()
        index = self.case_one_combo.findData(value)
        if index >= 0:
            self.case_one_combo.setCurrentIndex(index)

    def _set_stages(
        self, build: bool, sync: bool, ctest: bool, cases: bool, triage: bool
    ) -> None:
        self.applying_run_mode = True
        try:
            self.stage_build.setChecked(build)
            self.stage_sync.setChecked(sync)
            self.stage_ctest.setChecked(ctest)
            self.stage_cases.setChecked(cases)
            self.stage_triage.setChecked(triage)
        finally:
            self.applying_run_mode = False

    @Slot()
    def _apply_run_mode(self, *_args: object) -> None:
        if self.applying_run_mode:
            return
        mode = str(self.run_mode.currentData())
        if mode == "custom":
            self._update_mode_enabled_state(mode)
            self._update_plan_summary()
            return
        if mode == "tests_only":
            self.build_mode.setCurrentIndex(self.build_mode.findData("skip"))
            self._set_stages(False, True, True, True, True)
        elif mode == "build_only":
            if self.build_mode.currentData() == "skip":
                self.build_mode.setCurrentIndex(self.build_mode.findData("incremental"))
            self._set_stages(True, False, False, False, False)
        else:
            if self.build_mode.currentData() == "skip":
                self.build_mode.setCurrentIndex(self.build_mode.findData("incremental"))
            self._set_stages(True, True, True, True, True)
        self._update_mode_enabled_state(mode)
        self._update_plan_summary()

    def _update_mode_enabled_state(self, mode: str) -> None:
        tests_only = mode == "tests_only"
        build_only = mode == "build_only"
        for widget in (self.build_mode, self.build_target, self.build_runner):
            widget.setEnabled(not tests_only)
        self.stage_build.setEnabled(not tests_only and not build_only)
        self.test_panel.setEnabled(not build_only)
        for widget in (
            self.stage_sync,
            self.stage_ctest,
            self.stage_cases,
            self.stage_triage,
            self.opt_o0,
            self.opt_o2,
            self.keep_temps,
        ):
            widget.setEnabled(not build_only)
        # El paralelismo se usa tanto al construir como al ejecutar pruebas.
        self.jobs.setEnabled(True)

    @Slot()
    def _stage_selection_changed(self, *_args: object) -> None:
        mode = str(self.run_mode.currentData())
        if not self.applying_run_mode and mode != "tests_only":
            custom = self.run_mode.findData("custom")
            if custom >= 0 and self.run_mode.currentIndex() != custom:
                self.run_mode.setCurrentIndex(custom)
        self._update_plan_summary()

    def _run_one_ctest_without_build(self) -> None:
        self._prepare_one_ctest(False)

    def _run_one_ctest_with_build(self) -> None:
        self._prepare_one_ctest(True)

    def _prepare_one_ctest(self, build_compiler: bool) -> None:
        if self.ctest_one_combo.count() == 0:
            QMessageBox.warning(self, APP_NAME, "No hay pruebas CTest disponibles en este build.")
            return
        self.ctest_scope.setCurrentIndex(self.ctest_scope.findData("one"))
        self.build_mode.setCurrentIndex(
            self.build_mode.findData("incremental" if build_compiler else "skip")
        )
        self.build_target.setCurrentIndex(self.build_target.findData("auto"))
        if build_compiler:
            self._set_stages(True, True, True, False, False)
            self.run_mode.setCurrentIndex(self.run_mode.findData("custom"))
        else:
            self.run_mode.setCurrentIndex(self.run_mode.findData("tests_only"))
            self._set_stages(False, True, True, False, False)
        self.status_label.setText("Plan preparado; pulsa Iniciar")
        self._update_plan_summary()

    def _run_one_case_without_build(self) -> None:
        self._prepare_one_case(False)

    def _run_one_case_with_build(self) -> None:
        self._prepare_one_case(True)

    def _prepare_one_case(self, build_compiler: bool) -> None:
        if self.case_one_combo.count() == 0:
            QMessageBox.warning(self, APP_NAME, "No hay fixtures o ejemplos disponibles.")
            return
        self.case_scope.setCurrentIndex(self.case_scope.findData("one"))
        self.build_mode.setCurrentIndex(
            self.build_mode.findData("incremental" if build_compiler else "skip")
        )
        self.build_target.setCurrentIndex(self.build_target.findData("auto"))
        if build_compiler:
            self._set_stages(True, True, False, True, False)
            self.run_mode.setCurrentIndex(self.run_mode.findData("custom"))
        else:
            self.run_mode.setCurrentIndex(self.run_mode.findData("tests_only"))
            self._set_stages(False, True, False, True, False)
        self.status_label.setText("Plan preparado; pulsa Iniciar")
        self._update_plan_summary()

    def _resolved_build_target(self, selected_ctests: list[str]) -> str:
        requested = str(self.build_target.currentData())
        if requested != "auto":
            return requested
        if self.stage_ctest.isChecked():
            if str(self.ctest_scope.currentData()) == "one" and len(selected_ctests) == 1:
                test_target = self.ctest_targets.get(selected_ctests[0], "")
                if test_target:
                    return f"nsc;{test_target}" if test_target != "nsc" else "nsc"
            return "all"
        return "nsc"

    def _update_plan_summary(self, *_args: object) -> None:
        if not hasattr(self, "plan_summary") or not hasattr(self, "ctest_scope"):
            return
        selected_ctests = self._selected_ctest_values()
        target = self._resolved_build_target(selected_ctests)
        runner = self.build_runner.currentText()
        if not self.stage_build.isChecked() or self.build_mode.currentData() == "skip":
            build_sentence = "No se reconstruirá ni instalará nsc."
        elif target == "all":
            build_sentence = (
                f"Se construirán nsc y todos los binarios de CTest usando {runner}."
            )
        else:
            build_sentence = (
                f"Se construirá {' + '.join(target.split(';'))} usando {runner}."
            )
        tests: list[str] = []
        if self.stage_ctest.isChecked():
            tests.append(
                "CTest completo"
                if not selected_ctests
                else f"{len(selected_ctests)} prueba(s) CTest"
            )
        selected_cases = self._selected_case_values()
        if self.stage_cases.isChecked():
            tests.append(
                "todos los fixtures"
                if not selected_cases
                else f"{len(selected_cases)} fixture(s)"
            )
        if self.stage_triage.isChecked():
            tests.append("bug triage")
        test_text = ", ".join(tests) if tests else "ninguna prueba"
        linker_text = self.linker_mode.currentText()
        if tests:
            test_sentence = (
                f"Se ejecutará: {test_text}. Los programas de prueba se enlazarán con "
                f"{linker_text}."
            )
        else:
            test_sentence = "No se ejecutarán pruebas."
        self.plan_summary.setText(f"Plan: {build_sentence} {test_sentence}")

    def _load_settings(self) -> None:
        self.build_path.setText(self.settings.value("build_path", str(DEFAULT_BUILD)))
        self.prefix_path.setText(self.settings.value("prefix_path", str(DEFAULT_PREFIX)))
        self.llvm_path.setText(self.settings.value("llvm_path", str(DEFAULT_LLVM)))
        self.nslink_path.setText(self.settings.value("nslink_path", str(DEFAULT_NSLINK)))
        self.jobs.setValue(int(self.settings.value("jobs", min(os.cpu_count() or 4, 16))))
        runner_index = self.build_runner.findData(self.settings.value("build_runner", "cmake"))
        if runner_index >= 0:
            self.build_runner.setCurrentIndex(runner_index)
        linker_index = int(self.settings.value("linker_index", 0))
        if 0 <= linker_index < self.linker_mode.count():
            self.linker_mode.setCurrentIndex(linker_index)
        run_mode_index = self.run_mode.findData(
            self.settings.value("run_mode", "build_and_tests")
        )
        if run_mode_index >= 0:
            self.run_mode.setCurrentIndex(run_mode_index)
            self._apply_run_mode()
        geometry = self.settings.value("window_geometry")
        if geometry is not None:
            self.restoreGeometry(geometry)
        dock_state = self.settings.value("dock_state")
        if dock_state is not None:
            self.restoreState(dock_state, 1)

    def _save_settings(self) -> None:
        self.settings.setValue("build_path", self.build_path.text())
        self.settings.setValue("prefix_path", self.prefix_path.text())
        self.settings.setValue("llvm_path", self.llvm_path.text())
        self.settings.setValue("nslink_path", self.nslink_path.text())
        self.settings.setValue("jobs", self.jobs.value())
        self.settings.setValue("build_runner", self.build_runner.currentData())
        self.settings.setValue("linker_index", self.linker_mode.currentIndex())
        self.settings.setValue("run_mode", self.run_mode.currentData())
        self.settings.setValue("window_geometry", self.saveGeometry())
        self.settings.setValue("dock_state", self.saveState(1))

    def refresh_ctests(self) -> None:
        if self.inventory_process is not None:
            return
        build = Path(self.build_path.text().strip())
        if not (build / "CTestTestfile.cmake").is_file():
            self.status_label.setText("CTest: el build aún no está configurado")
            return
        self.status_label.setText("Leyendo inventario de CTest...")
        self.inventory_bytes = bytearray()
        process = QProcess(self)
        self.inventory_process = process
        process.setProgram("ctest")
        process.setArguments(["--test-dir", str(build), "--show-only=json-v1"])
        process.setProcessChannelMode(QProcess.ProcessChannelMode.MergedChannels)
        process.readyReadStandardOutput.connect(
            lambda: self.inventory_bytes.extend(bytes(process.readAllStandardOutput()))
        )
        process.finished.connect(self._inventory_finished)
        process.start()

    @Slot(int, QProcess.ExitStatus)
    def _inventory_finished(self, exit_code: int, _status: QProcess.ExitStatus) -> None:
        process = self.inventory_process
        if process is not None:
            self.inventory_bytes.extend(bytes(process.readAllStandardOutput()))
        self.inventory_process = None
        if exit_code != 0:
            self.status_label.setText("No se pudo leer CTest")
            return
        try:
            raw = self.inventory_bytes.decode("utf-8", errors="replace")
            # Algunas versiones de CTest imprimen primero
            # "Internal ctest changing into directory..." incluso en modo JSON.
            begin = raw.find("{")
            end = raw.rfind("}")
            if begin < 0 or end < begin:
                raise ValueError("CTest no devolvió un objeto JSON")
            payload = json.loads(raw[begin : end + 1])
            tests = [test for test in payload.get("tests", []) if "name" in test]
            names = sorted(str(test["name"]) for test in tests)
        except (ValueError, TypeError, KeyError):
            self.status_label.setText("CTest devolvió un inventario inválido")
            return
        self.ctest_targets.clear()
        self.ctest_groups.clear()
        for test in tests:
            name = str(test["name"])
            self.ctest_groups.setdefault(ctest_group(name), []).append(name)
            command = test.get("command", [])
            if command:
                executable = Path(str(command[0]))
                if executable.suffix.casefold() == ".exe" and (
                    executable.stem == "nsc" or executable.stem.startswith("nsc_")
                ):
                    self.ctest_targets[name] = executable.stem
        self.ctest_list.clear()
        self.ctest_one_combo.clear()
        for name in names:
            self.ctest_list.addItem(name)
            self.ctest_one_combo.addItem(name, name)
        self.ctest_group_combo.clear()
        for group in sorted(self.ctest_groups, key=str.casefold):
            members = sorted(self.ctest_groups[group], key=str.casefold)
            self.ctest_groups[group] = members
            self.ctest_group_combo.addItem(f"{group} ({len(members)})", group)
        self.status_label.setText(f"CTest: {len(names)} pruebas")
        self._update_ctest_scope()

    def _make_plan(self) -> RunPlan | None:
        optimizations = []
        if self.opt_o0.isChecked():
            optimizations.append("-O0")
        if self.opt_o2.isChecked():
            optimizations.append("-O2")
        if self.stage_cases.isChecked() and not optimizations:
            QMessageBox.warning(self, APP_NAME, "Selecciona O0, O2 o ambos.")
            return None
        selected_ctests = self._selected_ctest_values()
        selected_cases = self._selected_case_values()
        if self.stage_ctest.isChecked() and str(self.ctest_scope.currentData()) != "all" and not selected_ctests:
            QMessageBox.warning(self, APP_NAME, "El alcance de CTest elegido no contiene pruebas.")
            return None
        if self.stage_cases.isChecked() and str(self.case_scope.currentData()) != "all" and not selected_cases:
            QMessageBox.warning(self, APP_NAME, "El alcance de fixtures elegido no contiene casos.")
            return None
        linkers = list(self.linker_mode.currentData())
        return RunPlan(
            root=str(ROOT),
            build_dir=self.build_path.text().strip(),
            prefix=self.prefix_path.text().strip(),
            llvm_root=self.llvm_path.text().strip(),
            nslink_exe=self.nslink_path.text().strip(),
            build_mode=str(self.build_mode.currentData()),
            build_target=self._resolved_build_target(selected_ctests),
            build_runner=str(self.build_runner.currentData()),
            linkers=linkers,
            optimizations=optimizations,
            jobs=self.jobs.value(),
            run_build=self.stage_build.isChecked(),
            sync_libraries=self.stage_sync.isChecked(),
            run_ctest=self.stage_ctest.isChecked(),
            run_cases=self.stage_cases.isChecked(),
            run_triage=self.stage_triage.isChecked(),
            keep_link_temps=self.keep_temps.isChecked(),
            selected_ctests=selected_ctests,
            selected_cases=selected_cases,
        )

    @Slot()
    def start_run(self) -> None:
        if self.worker is not None:
            return
        plan = self._make_plan()
        if plan is None:
            return
        if not any((plan.run_build, plan.sync_libraries, plan.run_ctest, plan.run_cases, plan.run_triage)):
            QMessageBox.warning(self, APP_NAME, "Selecciona por lo menos una etapa.")
            return
        if plan.build_mode == "clean" and plan.run_build:
            answer = QMessageBox.question(
                self,
                APP_NAME,
                f"Se borrará por completo:\n{plan.build_dir}\n\n¿Continuar?",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
                QMessageBox.StandardButton.No,
            )
            if answer != QMessageBox.StandardButton.Yes:
                return
        if (
            plan.run_build
            and plan.build_mode != "skip"
            and plan.build_target == "nsc"
            and plan.run_ctest
        ):
            answer = QMessageBox.question(
                self,
                APP_NAME,
                "Se construirá sólo `nsc`, pero después se ejecutará CTest. Los binarios de "
                "prueba podrían estar ausentes o desactualizados.\n\n¿Continuar de todos modos?",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
                QMessageBox.StandardButton.No,
            )
            if answer != QMessageBox.StandardButton.Yes:
                return
        self._save_settings()
        for channel in self.output_channels:
            for optimization in self.output_optimizations:
                key = (channel, optimization)
                self.channel_consoles[key].clear()
                self.channel_error_tables[key].setRowCount(0)
                tabs = self.channel_inner_tabs[key]
                tabs.setTabText(self.channel_error_tab_indices[key], "Errores (0)")
                tabs.setCurrentIndex(0)
            self.channel_optimization_tabs[channel].setCurrentIndex(
                self.channel_optimization_indices[(channel, "General")]
            )
        self.output_channel_tabs.setCurrentIndex(self.channel_outer_indices["General"])
        self.stage_table.setRowCount(0)
        self.stage_row_by_name.clear()
        self.current_run_dir = None
        self.open_button.setEnabled(False)
        self.start_button.setEnabled(False)
        self.cancel_button.setEnabled(True)
        self.status_label.setText("Ejecutando...")

        thread = QThread(self)
        worker = PipelineWorker(plan, self.cases)
        worker.moveToThread(thread)
        thread.started.connect(worker.run)
        worker.output.connect(self.append_output)
        worker.stage_started.connect(self.on_stage_started)
        worker.stage_finished.connect(self.on_stage_finished)
        worker.failure_found.connect(self.on_failure_found)
        worker.progress.connect(self.on_progress)
        worker.finished.connect(self.on_finished)
        worker.finished.connect(thread.quit)
        worker.finished.connect(worker.deleteLater)
        thread.finished.connect(self._thread_finished)
        thread.finished.connect(thread.deleteLater)
        self.thread = thread
        self.worker = worker
        thread.start()

    @Slot()
    def cancel_run(self) -> None:
        if self.worker is not None:
            self.worker.cancel()
            self.cancel_button.setEnabled(False)

    def _known_output_channel(self, channel: str) -> str:
        return channel if channel in self.output_channels else "General"

    def _known_optimization(self, optimization: str) -> str:
        normalized = optimization.lstrip("-")
        return normalized if normalized in self.output_optimizations else "General"

    @Slot()
    def clear_current_console(self) -> None:
        index = self.output_channel_tabs.currentIndex()
        channel = next(
            (
                name
                for name, channel_index in self.channel_outer_indices.items()
                if channel_index == index
            ),
            "General",
        )
        optimization_tabs = self.channel_optimization_tabs[channel]
        optimization_index = optimization_tabs.currentIndex()
        optimization = next(
            (
                name
                for (key_channel, name), index_value in self.channel_optimization_indices.items()
                if key_channel == channel and index_value == optimization_index
            ),
            "General",
        )
        self.channel_consoles[(channel, optimization)].clear()

    @Slot(str, str, str, str)
    def append_output(self, line: str, level: str, channel: str, optimization: str) -> None:
        colors = {
            "info": QColor("#d6d6d6"),
            "command": QColor("#7dcfff"),
            "success": QColor("#9ece6a"),
            "warn": QColor("#e0af68"),
            "error": QColor("#f7768e"),
        }
        channel = self._known_output_channel(channel)
        optimization = self._known_optimization(optimization)
        key = (channel, optimization)
        console = self.channel_consoles[key]
        cursor = console.textCursor()
        cursor.movePosition(QTextCursor.MoveOperation.End)
        fmt = QTextCharFormat()
        fmt.setForeground(colors.get(level, colors["info"]))
        cursor.insertText(line + "\n", fmt)
        console.setTextCursor(cursor)
        console.ensureCursorVisible()
        self.output_channel_tabs.setCurrentIndex(self.channel_outer_indices[channel])
        self.channel_optimization_tabs[channel].setCurrentIndex(
            self.channel_optimization_indices[key]
        )

    @Slot(dict)
    def on_failure_found(self, failure: dict) -> None:
        channel = self._known_output_channel(str(failure.get("linker", "General")))
        optimization = self._known_optimization(str(failure.get("optimization", "General")))
        key = (channel, optimization)
        error_table = self.channel_error_tables[key]
        row = error_table.rowCount()
        error_table.insertRow(row)
        log_path = str(failure.get("error_log", ""))
        values = (
            str(failure.get("linker", "—")),
            str(failure.get("source", "")),
            str(failure.get("detail", "")),
            Path(log_path).name if log_path else "",
        )
        for column, value in enumerate(values):
            item = QTableWidgetItem(value)
            item.setData(Qt.ItemDataRole.UserRole, log_path)
            item.setToolTip(f"Abrir {log_path}" if log_path else "No hay log asociado")
            error_table.setItem(row, column, item)
        count = error_table.rowCount()
        inner_tabs = self.channel_inner_tabs[key]
        error_index = self.channel_error_tab_indices[key]
        inner_tabs.setTabText(error_index, f"Errores ({count})")
        inner_tabs.setCurrentIndex(error_index)
        self.output_channel_tabs.setCurrentIndex(self.channel_outer_indices[channel])
        self.channel_optimization_tabs[channel].setCurrentIndex(
            self.channel_optimization_indices[key]
        )

    @Slot(QTableWidgetItem)
    def open_error_log(self, item: QTableWidgetItem) -> None:
        log_path = Path(str(item.data(Qt.ItemDataRole.UserRole) or ""))
        if log_path.is_file():
            QDesktopServices.openUrl(QUrl.fromLocalFile(str(log_path)))
            return
        QMessageBox.warning(self, APP_NAME, f"El log no existe:\n{log_path}")

    @Slot(str)
    def on_stage_started(self, name: str) -> None:
        row = self.stage_table.rowCount()
        self.stage_table.insertRow(row)
        self.stage_row_by_name[name] = row
        self.stage_table.setItem(row, 0, QTableWidgetItem(name))
        self.stage_table.setItem(row, 1, QTableWidgetItem("ejecutando"))
        self.stage_table.setItem(row, 2, QTableWidgetItem(""))
        self.stage_table.setItem(row, 3, QTableWidgetItem(""))
        self.status_label.setText(name)

    @Slot(str, str, float, int)
    def on_stage_finished(self, name: str, status: str, seconds: float, exit_code: int) -> None:
        row = self.stage_row_by_name.get(name)
        if row is None:
            return
        self.stage_table.item(row, 1).setText(status)
        self.stage_table.item(row, 2).setText(f"{seconds:.3f} s")
        self.stage_table.item(row, 3).setText(str(exit_code))

    @Slot(int, int)
    def on_progress(self, value: int, total: int) -> None:
        self.progress_bar.setRange(0, max(total, 1))
        self.progress_bar.setValue(min(value, total))

    @Slot(dict)
    def on_finished(self, summary: dict) -> None:
        self.current_run_dir = Path(summary["run_dir"])
        self.start_button.setEnabled(True)
        self.cancel_button.setEnabled(False)
        self.open_button.setEnabled(True)
        self.status_label.setText(
            f"{summary['status']} — {summary['seconds']:.2f} s — {summary['failed']} fallo(s)"
        )

    @Slot()
    def _thread_finished(self) -> None:
        self.worker = None
        self.thread = None
        if self.close_after_cancel:
            QTimer.singleShot(0, self.close)

    @Slot()
    def open_results(self) -> None:
        if self.current_run_dir is not None:
            QDesktopServices.openUrl(QUrl.fromLocalFile(str(self.current_run_dir)))

    def closeEvent(self, event) -> None:  # type: ignore[override]
        if self.worker is not None:
            answer = QMessageBox.question(
                self,
                APP_NAME,
                "Hay una ejecución activa. ¿Cancelar y cerrar?",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
                QMessageBox.StandardButton.No,
            )
            if answer != QMessageBox.StandardButton.Yes:
                event.ignore()
                return
            self.close_after_cancel = True
            self.worker.cancel()
            event.ignore()
            return
        self._save_settings()
        event.accept()


def dark_stylesheet() -> str:
    return """
    QWidget { background: #1e1e2e; color: #cdd6f4; font-size: 10pt; }
    QMainWindow, QDialog { background: #181825; }
    QGroupBox { border: 1px solid #45475a; border-radius: 6px; margin-top: 10px; padding-top: 8px; }
    QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; color: #89b4fa; }
    QLineEdit, QComboBox, QSpinBox, QListWidget, QTextEdit, QTableWidget {
        background: #11111b; border: 1px solid #45475a; border-radius: 4px; padding: 4px;
        selection-background-color: #45475a; selection-color: #f5e0dc;
    }
    QPushButton { background: #313244; border: 1px solid #585b70; border-radius: 4px; padding: 6px 12px; }
    QPushButton:hover { background: #45475a; }
    QPushButton:pressed { background: #585b70; }
    QPushButton:disabled { color: #6c7086; background: #252535; }
    QHeaderView::section { background: #313244; color: #cdd6f4; padding: 5px; border: 0; }
    QTabBar::tab { background: #181825; padding: 7px 14px; border: 1px solid #313244; }
    QTabBar::tab:selected { background: #313244; color: #89b4fa; }
    QProgressBar { border: 1px solid #45475a; border-radius: 4px; text-align: center; background: #11111b; }
    QProgressBar::chunk { background: #89b4fa; }
    QDockWidget { color: #cdd6f4; }
    QDockWidget::title {
        background: #313244; color: #89b4fa; padding: 6px; text-align: left;
    }
    QLabel#muted { color: #a6adc8; font-size: 9pt; }
    QLabel#planSummary {
        background: #181825; border: 1px solid #45475a; border-radius: 5px;
        color: #bac2de; padding: 7px;
    }
    """


def main() -> int:
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--help", action="store_true")
    args, qt_args = parser.parse_known_args()
    if args.help:
        print(__doc__)
        return 0
    app = QApplication([sys.argv[0], *qt_args])
    app.setApplicationName(APP_NAME)
    app.setStyle("Fusion")
    app.setStyleSheet(dark_stylesheet())
    window = MainWindow()
    window.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
