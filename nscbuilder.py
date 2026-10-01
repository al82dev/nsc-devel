#!/usr/bin/env python3
"""GUI de construcción y pruebas para NativeSharp.

Requisitos:
    python -m pip install PySide6

Uso automatizado:
    python nscbuilder.py --baseline [--jobs N] [opciones de rutas]

Las rutas aceptan la precedencia CLI, entorno, archivo JSON y valor predeterminado. Consulte
``python nscbuilder.py --help`` para ver las opciones y variables disponibles.

Los resultados se guardan como archivos ordinarios bajo:
    <repo>\\_nscbuilder\\history\\<fecha-id>\\

Los productos de construcción permanecen físicamente separados:
    <repo>\\build-clang\\            compilador nsc
    <repo>\\build-test-lldlinker\\   CTest para la pasada LLD-link
    <repo>\\build-test-nslink\\      CTest para la pasada NSLink
    <repo>\\_nscbuilder\\work\\tests\\ programas generados por nsc

No usa una base de datos. Cada corrida deja summary.json, metadata.json, console.log,
report.md y logs separados por etapa y por caso. La interfaz reconstruye el historial de las
corridas anteriores sin modificar su evidencia.
"""

from __future__ import annotations

import argparse
import ctypes
import hashlib
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
import xml.etree.ElementTree as ET
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import asdict, dataclass
from datetime import datetime
from pathlib import Path
from typing import Callable, Iterable


# El runner de consola y los tests no necesitan Qt. Importar PySide6 en esos modos hacía que un
# problema nativo de shiboken6 derribara también `--baseline` antes de procesar un solo argumento.
# Cuando este archivo se importa como módulo, o se ejecuta con --baseline/--help, se proporcionan
# únicamente las primitivas de señales que usa PipelineWorker. La GUI conserva los imports reales.
HEADLESS = __name__ != "__main__" or any(arg in ("--baseline", "--help") for arg in sys.argv[1:])

if HEADLESS:
    class _HeadlessBoundSignal:
        def __init__(self) -> None:
            self.callbacks: list[Callable[..., object]] = []

        def connect(self, callback: Callable[..., object]) -> None:
            self.callbacks.append(callback)

        def emit(self, *args: object) -> None:
            for callback in tuple(self.callbacks):
                callback(*args)


    class Signal:  # type: ignore[no-redef]
        def __init__(self, *_types: object) -> None:
            self.name = ""

        def __set_name__(self, _owner: type, name: str) -> None:
            self.name = f"__headless_signal_{name}"

        def __get__(self, instance: object, _owner: type | None = None) -> object:
            if instance is None:
                return self
            values = vars(instance)
            if self.name not in values:
                values[self.name] = _HeadlessBoundSignal()
            return values[self.name]


    def Slot(*_types: object, **_options: object):  # type: ignore[no-redef]
        def decorate(function: Callable[..., object]) -> Callable[..., object]:
            return function

        return decorate


    class QObject:  # type: ignore[no-redef]
        def __init__(self, *_args: object, **_kwargs: object) -> None:
            pass


    class _GuiUnavailableMeta(type):
        def __getattr__(cls, _name: str) -> type:
            return cls


    class _GuiUnavailable(metaclass=_GuiUnavailableMeta):
        pass


    QMainWindow = _GuiUnavailable
    QProcess = QSettings = QThread = QTimer = Qt = QUrl = _GuiUnavailable
    QColor = QDesktopServices = QFontDatabase = QKeySequence = QShortcut = _GuiUnavailable
    QTextCharFormat = QTextCursor = _GuiUnavailable
    QAbstractItemView = QApplication = QCheckBox = QComboBox = QDockWidget = _GuiUnavailable
    QFormLayout = QGridLayout = QGroupBox = QHBoxLayout = QHeaderView = QLabel = _GuiUnavailable
    QLineEdit = QListWidget = QListWidgetItem = QMessageBox = QPlainTextEdit = _GuiUnavailable
    QProgressBar = QPushButton = QSpinBox = QTableWidget = QTableWidgetItem = _GuiUnavailable
    QTabWidget = QTextEdit = QVBoxLayout = QWidget = _GuiUnavailable
else:
    try:
        from PySide6.QtCore import QObject, QProcess, QSettings, QThread, QTimer, Qt, QUrl, Signal, Slot
        from PySide6.QtGui import (
            QColor,
            QDesktopServices,
            QFontDatabase,
            QKeySequence,
            QShortcut,
            QTextCharFormat,
            QTextCursor,
        )
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
DEFAULT_TEST_BUILD_LLD = ROOT / "build-test-lldlinker"
DEFAULT_TEST_BUILD_NSLINK = ROOT / "build-test-nslink"
DEFAULT_PREFIX = Path(r"C:\native-sharp")
DEFAULT_BASELINE_PREFIX = Path(r"A:\native-sharp-3.0-baseline")
DEFAULT_LLVM = Path(r"C:\clang-llvm")
DEFAULT_NSLINK = Path(r"A:\nslink-devel\build-debug\bin\nslink.exe")
HISTORY_ROOT = ROOT / "_nscbuilder" / "history"
DEFAULT_TEST_WORK_ROOT = ROOT / "_nscbuilder" / "work" / "tests"
DEFAULT_TEMP_ROOT = ROOT / "_nscbuilder" / "temp"
LIBRARY_SOURCE_MARKER = b"Synced from configured NativeSharp installation\r\n"

PATH_ENVIRONMENT: dict[str, tuple[str, ...]] = {
    "prefix": ("NSC_INSTALL_ROOT", "NS_ROOT"),
    "baseline_prefix": ("NSC_BASELINE_ROOT",),
    "build_dir": ("NSC_BUILD_ROOT",),
    "test_build_lld": ("NSC_TEST_BUILD_LLD",),
    "test_build_nslink": ("NSC_TEST_BUILD_NSLINK",),
    "llvm_root": ("NSC_LLVM_ROOT",),
    "nslink": ("NSLINK_EXE",),
    "history_root": ("NSC_HISTORY_ROOT",),
    "work_root": ("NSC_WORK_ROOT",),
    "temp_root": ("NSC_TEMP_ROOT",),
}


def load_builder_config(path: str | Path | None) -> dict:
    """Carga un archivo de rutas versionado sin aplicar todavía precedencias."""
    if path is None or not str(path).strip():
        return {}
    config_path = Path(path)
    value = json.loads(config_path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ValueError("la configuración debe ser un objeto JSON")
    if value.get("schema") != "nscbuilder-paths" or value.get("version") != 1:
        raise ValueError("se esperaba schema 'nscbuilder-paths', version 1")
    paths = value.get("paths", {})
    roots = value.get("write_roots", [])
    if not isinstance(paths, dict) or not all(isinstance(key, str) for key in paths):
        raise ValueError("'paths' debe ser un objeto")
    if not isinstance(roots, list) or not all(isinstance(root, str) for root in roots):
        raise ValueError("'write_roots' debe ser una lista de rutas")
    return value


def configured_path(config: dict, key: str, fallback: str | Path) -> str:
    """Resuelve entorno → archivo → predeterminado; la CLI se aplica después por argparse."""
    for name in PATH_ENVIRONMENT[key]:
        value = os.environ.get(name, "").strip()
        if value:
            return value
    value = config.get("paths", {}).get(key)
    return str(value) if isinstance(value, str) and value.strip() else str(fallback)


def configured_write_roots(config: dict) -> list[str]:
    """Obtiene las raíces autorizadas desde el entorno o el archivo de configuración."""
    environment = os.environ.get("NSC_WRITE_ROOTS", "").strip()
    if environment:
        return [part for part in environment.split(os.pathsep) if part.strip()]
    return list(config.get("write_roots", []))


def configured_build_id(config: dict) -> str:
    """Resuelve la identidad reproducible reservada para baselines y bootstrap."""
    value = os.environ.get("NSC_BUILD_ID_OVERRIDE", "").strip()
    if not value:
        configured = config.get("build_id_override", "")
        value = configured.strip() if isinstance(configured, str) else ""
    if value and not re.fullmatch(r"[0-9a-f]{16}", value):
        raise ValueError("build_id_override debe tener 16 dígitos hexadecimales minúsculos")
    return value


def configure_windows_error_mode() -> None:
    """Keep child-process failures in the logs instead of Windows dialog boxes."""
    if os.name != "nt":
        return

    # Child processes inherit this process-wide mode unless explicitly created with
    # CREATE_DEFAULT_ERROR_MODE.  The pipeline intentionally does not use that flag.
    sem_fail_critical_errors = 0x0001
    sem_no_gp_fault_error_box = 0x0002
    sem_no_open_file_error_box = 0x8000
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.SetErrorMode.argtypes = [ctypes.c_uint]
    kernel32.SetErrorMode.restype = ctypes.c_uint
    requested = (
        sem_fail_critical_errors
        | sem_no_gp_fault_error_box
        | sem_no_open_file_error_box
    )
    previous = kernel32.SetErrorMode(requested)
    kernel32.SetErrorMode(previous | requested)

    # SetErrorMode covers loader errors and unhandled faults inherited by descendants.  Windows
    # Error Reporting has a second, process-local UI switch, however, and may still offer its own
    # modal "stopped working" dialog for this process.  Keep that path non-interactive too.  The
    # call is deliberately optional so the builder continues to work on stripped Windows images
    # where wer.dll is unavailable.
    try:
        wer = ctypes.WinDLL("wer", use_last_error=True)
        wer.WerSetFlags.argtypes = [ctypes.c_ulong]
        wer.WerSetFlags.restype = ctypes.c_long
        wer.WerSetFlags(0x20)  # WER_FAULT_REPORTING_NO_UI
    except (AttributeError, OSError):
        pass


configure_windows_error_mode()
# Gate 0 del plan 123. Todo lo que produce la certificacion ABI vive bajo `_abi`, separado del
# historial ordinario de corridas: `_nscbuilder` se puede borrar sin perder un baseline.
ABI_ROOT = ROOT / "_abi"
ABI_GATES_ROOT = ABI_ROOT / "gates"
ABI_ARCHIVES_ROOT = ABI_ROOT / "archives"
# El manifiesto de fuentes del Gate 0. El plan 123 fija que se incluye y que se excluye; se escribe
# aqui una sola vez para que la respuesta no dependa de quien lance la accion.
ABI_SOURCE_DIRECTORIES: tuple[str, ...] = (
    "src",
    "include",
    "lib",
    "tests",
    "examples",
    "spec",
    "docs",
    "selfhost",
    "tools",
)
ABI_SOURCE_FILES: tuple[str, ...] = (
    "CMakeLists.txt",
    "nscbuilder.py",
    "cases_table.py",
)
ABI_SOURCE_EXCLUDED_NAMES: frozenset[str] = frozenset(
    {".git", "_abi", "_nscbuilder", "__pycache__", ".vs", ".vscode", ".cache"}
)
ABI_SOURCE_EXCLUDED_SUFFIXES: tuple[str, ...] = (".obj", ".pdb", ".ilk", ".exp", ".tmp", ".log")
ABI_SOURCE_EXCLUDED_PREFIXES: tuple[tuple[str, ...], ...] = (
    ("selfhost", "out"),
    # La evidencia describe una corrida del arbol, pero no forma parte de sus fuentes. Incluir un
    # gate que contiene el propio source_digest crearia una identidad autorreferente imposible de
    # cerrar; runs y gates quedan versionados/auditables, pero fuera del digest de entrada.
    ("selfhost", "evidence"),
)
TEST_WORK_ROOT = DEFAULT_TEST_WORK_ROOT
CLEANUP_BUILD_NAMES: tuple[str, ...] = (
    "build-clang",
    "build-clang-lldlink",
    "build-clang-nslink",
    "build-msvc",
    "build-test-lldlinker",
    "build-test-nslink",
    "build-tests",
)
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
    operation: str
    root: str
    compiler_build_dir: str
    prefix: str
    deploy_prefix: str
    llvm_root: str
    nslink_exe: str
    history_root: str
    test_work_root: str
    temp_root: str
    allowed_write_roots: list[str]
    run_id: str
    build_id_override: str
    host_linkage: str
    host_optimization: str
    build_mode: str
    build_target: str
    build_runner: str
    # CONSTRUIR LOS ARBOLES DE PRUEBA ES UNA DECISION APARTE, con su propio campo en vez de
    # deducirse de la operacion. La pestana del LENGUAJE lo pone en falso SIEMPRE: ahi no se
    # compila nada, se usa el `nsc.exe` ya instalado.
    build_test_trees: bool
    # El objetivo de ninja DENTRO del arbol de pruebas, separado del del compilador. Compartirlos
    # hacia que el baseline pidiera `nsc` en el arbol de pruebas --que no es ninguno de sus 128
    # ejecutables-- y solo se salvaba porque ya estaban construidos de antes.
    test_build_target: str
    test_build_mode: str
    test_build_dirs: dict[str, str]
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


def make_baseline_plan(
    jobs: int,
    *,
    compiler_build_dir: str | Path = DEFAULT_BUILD,
    prefix: str | Path = DEFAULT_BASELINE_PREFIX,
    deploy_prefix: str | Path = DEFAULT_PREFIX,
    llvm_root: str | Path = DEFAULT_LLVM,
    nslink_exe: str | Path = DEFAULT_NSLINK,
    test_build_lld: str | Path = DEFAULT_TEST_BUILD_LLD,
    test_build_nslink: str | Path = DEFAULT_TEST_BUILD_NSLINK,
    history_root: str | Path = HISTORY_ROOT,
    test_work_root: str | Path = DEFAULT_TEST_WORK_ROOT,
    temp_root: str | Path = DEFAULT_TEMP_ROOT,
    allowed_write_roots: Iterable[str | Path] = (),
    run_id: str = "",
    build_id_override: str = "",
    build_runner: str = "ninja",
) -> RunPlan:
    """Construye el plan inmutable de G0.2 compartido por GUI y consola."""
    return RunPlan(
        operation="baseline",
        root=str(ROOT),
        compiler_build_dir=str(compiler_build_dir),
        prefix=str(prefix),
        deploy_prefix=str(deploy_prefix),
        llvm_root=str(llvm_root),
        nslink_exe=str(nslink_exe),
        history_root=str(history_root),
        test_work_root=str(test_work_root),
        temp_root=str(temp_root),
        allowed_write_roots=[str(path) for path in allowed_write_roots],
        run_id=run_id,
        build_id_override=build_id_override,
        host_linkage="STATIC",
        host_optimization="O2",
        build_mode="incremental",
        build_target="nsc",
        build_runner=build_runner,
        build_test_trees=True,
        test_build_target="pruebas",
        test_build_mode="incremental",
        test_build_dirs={
            "lld-link": str(test_build_lld),
            "NSLink": str(test_build_nslink),
        },
        linkers=["lld-link", "NSLink"],
        optimizations=["-O0", "-O1", "-O2", "-O3"],
        jobs=jobs,
        run_build=True,
        sync_libraries=False,
        run_ctest=True,
        run_cases=True,
        run_triage=True,
        keep_link_temps=False,
        selected_ctests=[],
        selected_cases=[],
    )


def path_is_within(path: Path, root: Path) -> bool:
    """Indica si ``path`` queda dentro de ``root`` después de resolver ambas rutas."""
    try:
        path.resolve().relative_to(root.resolve())
        return True
    except ValueError:
        return False


def reset_baseline_prefix(path: Path) -> None:
    """Crea un staging limpio sin permitir que una ruta configurable amplíe el borrado."""
    resolved = path.resolve()
    expected = DEFAULT_BASELINE_PREFIX.resolve()
    if resolved != expected:
        raise ValueError(
            "el staging del baseline debe ser exactamente "
            f"{expected}; se recibió {resolved}"
        )
    if resolved.exists():
        shutil.rmtree(resolved)
    resolved.mkdir(parents=True)


def validate_baseline_nslink_source(source: Path, prefix: Path) -> Path:
    """Valida que NSLink venga de una construcción en A: y no del staging que se vacía."""
    resolved = source.resolve()
    staging = prefix.resolve()
    if resolved.drive.casefold() != "a:":
        raise ValueError(
            "NSLink para el baseline debe provenir de una construcción en A:; "
            f"se recibió {resolved}"
        )
    if path_is_within(resolved, staging):
        raise ValueError(
            "NSLink para el baseline no puede provenir del staging que será reiniciado: "
            f"{resolved}"
        )
    if not resolved.is_file():
        raise FileNotFoundError(f"no existe el NSLink construido en A: {resolved}")
    return resolved


def stage_baseline_nslink(source: Path, prefix: Path) -> Path:
    """Copia NSLink de A: al staging fijo y comprueba que el contenido sea idéntico."""
    staging = prefix.resolve()
    expected = DEFAULT_BASELINE_PREFIX.resolve()
    if staging != expected:
        raise ValueError(
            "el destino de NSLink debe ser exactamente el staging del baseline "
            f"{expected}; se recibió {staging}"
        )
    resolved_source = validate_baseline_nslink_source(source, staging)
    destination = staging / "bin" / "nslink.exe"
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(resolved_source, destination)
    if sha256_file(resolved_source) != sha256_file(destination):
        raise OSError(f"la copia de NSLink no coincide con su origen: {destination}")
    return destination


def promote_certified_baseline(source: Path, destination: Path, run_id: str) -> dict[str, str]:
    """Reemplaza C:\\native-sharp con el staging certificado, siempre en dirección A: -> C:."""
    source = source.resolve()
    destination = destination.resolve()
    if source != DEFAULT_BASELINE_PREFIX.resolve():
        raise ValueError(f"origen de promoción no autorizado: {source}")
    if destination != DEFAULT_PREFIX.resolve():
        raise ValueError(f"destino de promoción no autorizado: {destination}")
    if not (source / "bin" / "nsc.exe").is_file():
        raise FileNotFoundError(f"el staging no contiene bin\\nsc.exe: {source}")

    # C:\native-sharp también contiene componentes instalados por otros repositorios (bindings,
    # linker, debugger y Saru). La promoción del compilador debe sobrescribir lo certificado por
    # este staging, pero nunca borrar esos componentes ajenos.
    destination.mkdir(parents=True, exist_ok=True)
    shutil.copytree(source, destination, dirs_exist_ok=True)
    return {
        "source": str(source),
        "destination": str(destination),
        "nsc_sha256": sha256_file(destination / "bin" / "nsc.exe"),
    }


def validate_plan_write_paths(plan: RunPlan) -> list[str]:
    """Rechaza destinos de escritura que escapen de las raíces autorizadas de la corrida."""
    if not plan.allowed_write_roots:
        return []
    roots = [Path(path).resolve() for path in plan.allowed_write_roots]
    candidates = [
        Path(plan.compiler_build_dir),
        Path(plan.prefix),
        Path(plan.deploy_prefix),
        *(Path(path) for path in plan.test_build_dirs.values()),
        Path(plan.history_root),
        Path(plan.test_work_root),
        Path(plan.temp_root),
        ABI_ROOT,
    ]
    rejected: list[str] = []
    for candidate in candidates:
        if not str(candidate):
            continue
        resolved = candidate.resolve()
        if not any(path_is_within(resolved, root) for root in roots):
            rejected.append(str(resolved))
    return rejected


MANAGED_BUILD_MARKER = ".nscbuilder-managed-build"


def build_directory_allowed(
    path: Path,
    repository_root: Path,
    legacy_prefix: str,
    allowed_write_roots: Iterable[str | Path],
) -> bool:
    """Valida uso; borrar exige además el marcador cuando no es un build histórico."""
    resolved = path.resolve()
    repository = repository_root.resolve()
    if resolved.parent == repository and resolved.name.casefold().startswith(legacy_prefix):
        return True
    for root_value in allowed_write_roots:
        root = Path(root_value).resolve()
        if resolved != root and path_is_within(resolved, root):
            return True
    return False


def build_directory_can_be_deleted(
    path: Path,
    repository_root: Path,
    legacy_prefix: str,
) -> bool:
    resolved = path.resolve()
    repository = repository_root.resolve()
    legacy = resolved.parent == repository and resolved.name.casefold().startswith(legacy_prefix)
    return legacy or (resolved / MANAGED_BUILD_MARKER).is_file()


def mark_managed_build(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)
    (path / MANAGED_BUILD_MARKER).write_text(
        "NativeSharp nscbuilder managed build v1\n",
        encoding="utf-8",
    )


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


def cleanup_targets(configured_builds: Iterable[Path]) -> tuple[list[Path], list[Path]]:
    """Devuelve artefactos borrables y rutas rechazadas, siempre dentro del repositorio.

    Los builds configurados por el usuario sólo son aceptables como hijos directos de ``ROOT``
    cuyo nombre comienza con ``build-``. También se permiten exactamente las áreas temporales que
    usa el runner y los logs automáticos ``errorN.log`` en la raíz. Resolver primero la ruta
    impide que un enlace o junction escape del repositorio.
    """
    root = ROOT.resolve()
    test_work = TEST_WORK_ROOT.resolve()
    temp_root = DEFAULT_TEMP_ROOT.resolve()
    error_logs = sorted(
        (
            path
            for path in ROOT.iterdir()
            if path.is_file() and re.fullmatch(r"error(?:\d+)?\.log", path.name, re.IGNORECASE)
        ),
        key=lambda path: path.name.casefold(),
    )
    candidates = [
        *(Path(path) for path in configured_builds),
        *(ROOT / name for name in CLEANUP_BUILD_NAMES),
        TEST_WORK_ROOT,
        DEFAULT_TEMP_ROOT,
        *error_logs,
    ]
    accepted: list[Path] = []
    rejected: list[Path] = []
    seen: set[str] = set()
    for candidate in candidates:
        resolved = candidate.resolve()
        key = str(resolved).casefold()
        if key in seen:
            continue
        seen.add(key)
        is_build_tree = resolved.parent == root and resolved.name.casefold().startswith("build-")
        is_error_log = resolved.parent == root and bool(
            re.fullmatch(r"error(?:\d+)?\.log", resolved.name, re.IGNORECASE)
        )
        if resolved not in (test_work, temp_root) and not is_build_tree and not is_error_log:
            rejected.append(resolved)
            continue
        if resolved.exists():
            accepted.append(resolved)
    return accepted, rejected


def command_text(command: Iterable[str]) -> str:
    return subprocess.list2cmdline([str(part) for part in command])


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def write_run_bootstrap_lock(
    template: Path,
    output: Path,
    captured_artifacts: dict[str, Path],
) -> dict:
    """Congela los artefactos mutables del bootstrap para una corrida concreta.

    El lock versionado conserva la identidad del último bootstrap promovido. Un baseline nuevo no
    debe reescribirlo ni comparar el nsc recién instalado contra un hash histórico. En su lugar se
    crea una copia dentro de la evidencia de la corrida y sólo se actualizan las identidades que
    el propio plan seleccionó explícitamente (hoy nsc0 y NSLink). Las herramientas externas
    continúan usando, sin cambios, los hashes estrictos del lock versionado.
    """
    value = json.loads(template.read_text(encoding="utf-8"))
    if value.get("schema") != "nativesharp-bootstrap-lock" or value.get("version") != 1:
        raise ValueError(f"lock de bootstrap no compatible: {template}")
    artifacts = value.get("artifacts")
    if not isinstance(artifacts, list):
        raise ValueError(f"lock de bootstrap sin artefactos: {template}")
    pending = set(captured_artifacts)
    for item in artifacts:
        if not isinstance(item, dict):
            continue
        identifier = item.get("id")
        if identifier not in captured_artifacts:
            continue
        artifact = captured_artifacts[str(identifier)].resolve()
        if not artifact.is_file():
            raise FileNotFoundError(f"artefacto de bootstrap ausente: {artifact}")
        item["sha256"] = sha256_file(artifact)
        pending.remove(str(identifier))
    if pending:
        raise ValueError("artefactos ausentes del lock de bootstrap: " + ", ".join(sorted(pending)))
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix(output.suffix + ".tmp")
    temporary.write_text(
        json.dumps(value, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    temporary.replace(output)
    return value


def canonical_manifest(root: Path, files: Iterable[Path]) -> list[dict]:
    """Manifiesto canonico del plan 123 §4.3: (ruta relativa, tamano, sha256).

    Las reglas no son adorno. Las rutas van relativas y con `/` para que el manifiesto no dependa
    de la unidad ni del sistema; el orden es BYTE A BYTE sobre UTF-8 y no el de `str` de Python,
    que ordena por punto de codigo y daria otro digest para el mismo contenido; y no entra ninguna
    fecha, porque un timestamp haria que el mismo arbol tuviera dos identidades.

    Dos rutas que solo difieran en mayusculas se RECHAZAN: en Windows son el mismo fichero, y un
    manifiesto que las liste por separado describiria un arbol que no existe.
    """
    entries: list[dict] = []
    seen: dict[str, str] = {}
    for path in files:
        relative = path.relative_to(root).as_posix()
        folded = relative.casefold()
        previous = seen.get(folded)
        if previous is not None and previous != relative:
            raise ValueError(
                f"Colision de rutas que solo difieren en mayusculas: {previous!r} y {relative!r}"
            )
        seen[folded] = relative
        entries.append(
            {"path": relative, "size": path.stat().st_size, "sha256": sha256_file(path)}
        )
    entries.sort(key=lambda row: row["path"].encode("utf-8"))
    return entries


def manifest_digest(entries: list[dict]) -> str:
    payload = json.dumps(entries, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
    return hashlib.sha256(payload.encode("utf-8")).hexdigest()


def collect_source_files(root: Path) -> list[Path]:
    """Los ficheros que forman la identidad del arbol para el Gate 0."""
    files: list[Path] = []
    for name in ABI_SOURCE_FILES:
        candidate = root / name
        if candidate.is_file():
            files.append(candidate)
    for name in ABI_SOURCE_DIRECTORIES:
        directory = root / name
        if not directory.is_dir():
            continue
        for path in directory.rglob("*"):
            if not path.is_file():
                continue
            relative_parts = path.relative_to(root).parts
            if any(
                tuple(part.casefold() for part in relative_parts[: len(prefix)]) == prefix
                for prefix in ABI_SOURCE_EXCLUDED_PREFIXES
            ):
                continue
            parts = set(relative_parts)
            if parts & ABI_SOURCE_EXCLUDED_NAMES:
                continue
            if any(part.casefold().startswith("build-") for part in parts):
                continue
            if path.suffix.casefold() in ABI_SOURCE_EXCLUDED_SUFFIXES:
                continue
            files.append(path)
    return files


def collect_tree_files(root: Path) -> list[Path]:
    return [path for path in root.rglob("*") if path.is_file()]


def write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, indent=2, ensure_ascii=False), encoding="utf-8")
    temporary.replace(path)


def structured_run_report(metadata: dict) -> dict:
    """Projecta metadata interna al contrato publico de reporte self-hosting v1."""
    failures = metadata.get("failures", [])
    problems = [
        str(row.get("detail") or row.get("kind") or "unspecified failure")
        for row in failures
    ]
    return {
        "schema": "nativesharp-selfhost-report",
        "version": 1,
        "run_id": str(metadata.get("id", "")),
        "started_at": str(metadata.get("started_at", "")),
        "finished_at": metadata.get("finished_at"),
        "status": str(metadata.get("status", "running")),
        "inputs": metadata.get("plan", {}),
        "stages": metadata.get("stages", []),
        "cases": metadata.get("cases", []),
        "failed": int(metadata.get("failed", len(failures))),
        "total_seconds": float(metadata.get("total_seconds", 0.0)),
        "problems": problems,
    }


def run_history_summary(metadata: dict, run_dir: Path) -> dict:
    """Crea el indice compacto e inmutable de una corrida terminada."""
    plan_value = metadata.get("plan", {})
    plan = plan_value if isinstance(plan_value, dict) else {}
    stages_value = metadata.get("stages", [])
    stages = stages_value if isinstance(stages_value, list) else []

    def stage_seconds(prefixes: tuple[str, ...]) -> float:
        total = 0.0
        for stage in stages:
            if not isinstance(stage, dict):
                continue
            name = str(stage.get("name", ""))
            if name.startswith(prefixes):
                try:
                    total += float(stage.get("seconds", 0.0))
                except (TypeError, ValueError):
                    continue
        return round(total, 3)

    linkers_value = plan.get("linkers", [])
    optimizations_value = plan.get("optimizations", [])
    return {
        "schema": "nativesharp-nscbuilder-history",
        "version": 1,
        "id": str(metadata.get("id", run_dir.name)),
        "started_at": str(metadata.get("started_at", "")),
        "finished_at": metadata.get("finished_at"),
        "status": str(metadata.get("status", "unknown")),
        "operation": str(plan.get("operation", "")),
        "total_seconds": float(metadata.get("total_seconds", 0.0)),
        "build_seconds": stage_seconds(
            ("Limpiar ", "Configurar ", "Construir", "Instalar", "Sincronizar ")
        ),
        "test_seconds": stage_seconds(("CTest ", "Fixtures ", "Bug triage ")),
        "certification_seconds": stage_seconds(("Certificar ",)),
        "failed": int(metadata.get("failed", 0)),
        "build_mode": str(plan.get("build_mode", "")),
        "test_build_mode": str(plan.get("test_build_mode", "")),
        "linkers": [str(value) for value in linkers_value]
            if isinstance(linkers_value, list) else [],
        "optimizations": [str(value) for value in optimizations_value]
            if isinstance(optimizations_value, list) else [],
        "run_dir": str(run_dir.resolve()),
        "report": str((run_dir / "report.md").resolve()),
    }


def load_run_history(history_root: Path) -> list[dict]:
    """Lee el historial nuevo y migra en memoria las corridas antiguas, sin reescribirlas."""
    if not history_root.is_dir():
        return []
    entries: list[dict] = []
    for run_dir in history_root.iterdir():
        if not run_dir.is_dir():
            continue
        summary_path = run_dir / "summary.json"
        metadata_path = run_dir / "metadata.json"
        try:
            if summary_path.is_file():
                value = json.loads(summary_path.read_text(encoding="utf-8", errors="replace"))
                if (
                    not isinstance(value, dict) or
                    value.get("schema") != "nativesharp-nscbuilder-history" or
                    value.get("version") != 1
                ):
                    continue
                entry = value
            elif metadata_path.is_file():
                metadata = json.loads(
                    metadata_path.read_text(encoding="utf-8", errors="replace")
                )
                if not isinstance(metadata, dict):
                    continue
                entry = run_history_summary(metadata, run_dir)
            else:
                continue
        except (OSError, ValueError, TypeError):
            continue
        entries.append(entry)
    entries.sort(
        key=lambda item: (str(item.get("started_at", "")), str(item.get("id", ""))),
        reverse=True,
    )
    return entries


def project_inputs(project: Path) -> list[Path]:
    """Devuelve fuentes y ProjectReference transitivos para el cache incremental."""
    pending = [project.resolve()]
    visited: set[Path] = set()
    inputs: set[Path] = set()
    while pending:
        current = pending.pop()
        if current in visited:
            continue
        visited.add(current)
        if not current.is_file():
            inputs.add(current)
            continue
        inputs.add(current)
        try:
            contents = current.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        for pattern in re.findall(r'<Compile\s+Include="([^"]+)"', contents, re.IGNORECASE):
            normalized = pattern.replace("\\", "/")
            for source in current.parent.glob(normalized):
                if source.is_file():
                    inputs.add(source.resolve())
        for reference in re.findall(
            r'<ProjectReference\s+Include="([^"]+)"', contents, re.IGNORECASE
        ):
            pending.append((current.parent / reference).resolve())
    return sorted(inputs, key=lambda path: str(path).casefold())


def incremental_fingerprint(project: Path, compiler: Path, linker: Path) -> str:
    digest = hashlib.sha256()
    for path in [*project_inputs(project), compiler.resolve(), linker.resolve()]:
        digest.update(str(path).casefold().encode("utf-8", errors="replace"))
        try:
            stat = path.stat()
        except OSError:
            digest.update(b"\0missing")
            continue
        digest.update(f"\0{stat.st_size}\0{stat.st_mtime_ns}".encode("ascii"))
    return digest.hexdigest()


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


# DOS CLASES DE PRUEBA, Y NO SE MEZCLAN.
#
# PRUEBA UNITARIA: un ejecutable de C++ que enlaza las librerias del compilador --nsc_frontend,
# nsc_llvm, nsc_compilation, nsc_target-- y mira el compilador POR DENTRO. Para ejecutarla hay que
# haberla compilado, porque es C++; no hay vuelta de hoja.
#
# PRUEBA DE LENGUAJE: un programa `.ns` que se compila CON `nsc.exe` y se ejecuta. Mira el
# compilador POR FUERA, usandolo. Para ejecutarla no hace falta compilar una sola linea de C++:
# basta el compilador ya instalado.
#
# POR QUE ESTABAN MEZCLADAS Y POR QUE ESO ERA CARO. Las dos viven en el mismo CTest, asi que
# pedir una prueba de lenguaje configuraba el arbol `build-test-*`, y ese arbol declara las
# librerias del compilador con sus fuentes: recompilaba el compilador entero para correr un
# programa de cinco lineas. Separarlas es lo que permite que la pestana del lenguaje no invoque
# jamas una compilacion.
#
# EL CASO QUE NO ES OBVIO: `nsc.cc1.correctness.*`. Son 47 pruebas DE LENGUAJE --compilan fixtures
# `.ns` con nsc.exe-- pero comparten un arnes de C++, `nsc_cc1_correctness_tests`, que si hay que
# tener construido. Van con las de lenguaje, que es lo que miden; si su arnes falta, la pestana lo
# DICE en vez de construirlo por su cuenta.
CTEST_HARNESS_PREFIXES = ("nsc.cc1.correctness",)

CTEST_UNIT = "unit"
CTEST_HARNESS = "harness"
CTEST_LANGUAGE = "language"

# NINGUN CONJUNTO ARRASTRA A OTRO. Esa es la regla, y de ella salen los tres:
#
#   UNIT     -- ejecutable de C++ propio, mide el compilador por dentro. Construye lo suyo.
#   HARNESS  -- mide el LENGUAJE pero cuelga de un binario de C++ compartido. Tiene una
#               dependencia que los otros no tienen, asi que va aparte en vez de contagiarsela a
#               nadie. Construye solo ese arnes.
#   LANGUAGE -- solo necesita el `nsc.exe` instalado. NO CONSTRUYE NADA, nunca.
#
# Un conjunto que necesitara algo distinto seria un cuarto conjunto, no una excepcion metida en
# uno de estos.


def classify_ctest(name: str, mapping: dict[str, str]) -> str:
    """A qué conjunto pertenece una prueba de CTest."""
    if name.startswith(CTEST_HARNESS_PREFIXES):
        return CTEST_HARNESS
    return CTEST_UNIT if ctest_target_for(name, mapping) else CTEST_LANGUAGE


# BUSCAR EN UNA LISTA DE 291 NOMBRES LARGOS Y PARECIDOS.
#
# El filtro era `subcadena` a secas, y con nombres como `nsc.stage56.c1-interop-completion` eso
# obliga a escribir el trozo EXACTO y contiguo. Tres reglas, todas baratas, lo cambian:
#
#   VARIOS TERMINOS, en cualquier orden, y todos tienen que aparecer: `stage56 interop` encuentra
#   `nsc.stage56.c1-interop-completion` sin acordarse de que hay un `.c1-` en medio.
#
#   EXCLUSION con `-`: `stage56 -bugfix` acota sin afinar el termino positivo.
#
#   SUBSECUENCIA cuando la subcadena falla: `s56c1` encuentra `...stage56.c1...`. Solo desde tres
#   caracteres; con uno o dos casaria casi todo y el filtro dejaria de filtrar.
def matches_filter(text: str, query: str) -> bool:
    """¿Casa `text` con lo escrito en el filtro? Query vacía casa con todo."""
    haystack = text.casefold()
    for term in query.casefold().split():
        negated = term.startswith("-") and len(term) > 1
        needle = term[1:] if negated else term
        if not needle:
            continue
        found = needle in haystack
        if not found and len(needle) >= 3:
            found = _is_subsequence(needle, haystack)
        if found == negated:
            return False
    return True


def _is_subsequence(needle: str, haystack: str) -> bool:
    """Las letras de `needle`, en orden, aunque no seguidas."""
    position = 0
    for character in needle:
        position = haystack.find(character, position)
        if position < 0:
            return False
        position += 1
    return True


# QUE EJECUTABLE CONSTRUYE CADA PRUEBA, LEIDO DEL CMakeLists Y NO DEL ARBOL.
#
# EL PROBLEMA QUE RESUELVE. El mapeo prueba -> objetivo salia solo del inventario de ctest
# (`--show-only=json-v1`), que exige un arbol YA CONFIGURADO. En cuanto se limpia el arbol de
# pruebas --que es justo lo que hay que hacer despues de tocar una cabecera-- ese inventario no
# existe, el mapeo queda vacio, y pedir dos pruebas concretas acababa construyendo las 101. Lo
# mismo pasaba la primera vez, antes de configurar nada.
#
# El CMakeLists SI lo dice, y lo dice sin construir ni configurar:
#
#     add_executable(nsc_llvm_optimization_tests tests/unit/LlvmOptimizationTests.cpp)
#     add_test(NAME nsc.llvm-optimization COMMAND nsc_llvm_optimization_tests)
#
# Solo se acepta como objetivo un COMMAND que sea un `add_executable` DECLARADO en el mismo
# fichero. Las 117 pruebas cuyo comando es `${NSC_TEST_COMPILER}` ejecutan el compilador instalado
# y no construyen nada del arbol de pruebas: para ellas no hay objetivo, y eso es un dato, no un
# fallo.
def ctest_targets_from_cmake(root: Path) -> dict[str, str]:
    """{nombre de prueba: objetivo} para las pruebas que tienen ejecutable propio."""
    cmake = root / "CMakeLists.txt"
    if not cmake.is_file():
        return {}
    text = cmake.read_text(encoding="utf-8", errors="replace")
    executables = set(re.findall(r"add_executable\(\s*([A-Za-z0-9_]+)", text))
    mapping: dict[str, str] = {}

    # LAS QUE PASAN POR EL AYUDANTE. `nsc_add_unit_test(<objetivo> <fuente> <nombre>)` hace por
    # dentro el `add_executable` y el `add_test`, asi que su `add_test(NAME ${test_name} ...)` no
    # dice ningun nombre concreto: los nombres estan en las 26 llamadas. Sin leerlas, un cuarto de
    # las pruebas unitarias quedaria sin objetivo conocido y pedir una sola volveria a construirlo
    # todo.
    for target, name in re.findall(
        r"nsc_add_unit_test\(\s*([A-Za-z0-9_]+)\s+\S+\s+([^\s)]+)\s*\)", text
    ):
        mapping[name] = target
    for name, command in re.findall(
        r"add_test\(\s*NAME\s+([^\s)]+)\s+COMMAND\s+([^\s)]+)", text
    ):
        # CADENA VACIA NO ES LO MISMO QUE AUSENTE. Vacia dice «se cual es esta prueba y no hay
        # nada que construir para ella»; ausente dice «no se que necesita», y eso si obliga a
        # construirlo todo. Sin esta distincion, pedir `nsc.version` --que corre el compilador
        # instalado-- volveria a construir los 101 ejecutables.
        target = command if command in executables else ""
        if "${" in name:
            # UN `add_test` DENTRO DE UN `foreach`. En el fichero se lee
            # `NAME nsc.cc1.correctness.${_cc1_label}`: un solo add_test que en tiempo de
            # configure se convierte en una prueba por fixture. Se guarda el prefijo con `*` para
            # que las 130 y pico que genera se resuelvan al mismo ejecutable. Sin esto, pedir UNA
            # fixture cc1 volvia a construirlo todo.
            prefix = name[: name.index("${")]
            if prefix:
                mapping[prefix + "*"] = target
            # Un nombre que EMPIEZA por `${` no deja prefijo alguno: es el `add_test` de dentro de
            # `nsc_add_unit_test`, y sus nombres reales ya se leyeron de las llamadas, arriba. Un
            # prefijo vacio casaria con cualquier prueba y daria un objetivo equivocado a todas.
            continue
        mapping[name] = target
    return mapping


# EL OBJETIVO DE UNA PRUEBA, resolviendo antes por nombre exacto y solo despues por prefijo. El
# orden importa: `nsc.cc1.correctness` es una prueba de pleno derecho --la de las comprobaciones
# de IR, la que corre con `--rest`-- y ademas es prefijo de las generadas en bucle. Entre varios
# prefijos gana el mas largo, que es el mas especifico.
def ctest_target_for(name: str, mapping: dict[str, str]) -> str | None:
    """El objetivo, `""` si la prueba no construye nada, o None si no se reconoce."""
    if name in mapping:
        return mapping[name]
    best_key = ""
    best_target: str | None = None
    for key, target in mapping.items():
        if not key.endswith("*"):
            continue
        prefix = key[:-1]
        if name.startswith(prefix) and len(prefix) >= len(best_key):
            best_key, best_target = prefix, target
    return best_target


# LA EXPRESION `-R` NO ES UN REGEX DE PYTHON, y confundirlas costo una corrida entera.
#
# MEDIDO el 2026-09-05. Esta funcion construia el filtro con `"^(?:" + "|".join(re.escape(n)) +
# ")$"`. ctest no usa el motor de Python sino `cmsys::RegularExpression`, que no conoce los grupos
# no capturantes ni el `\-` que `re.escape` produce. La respuesta fue:
#
#     RegularExpression::compile(): ?+* follows nothing.
#     No tests were found!!!
#
# --y codigo de salida 0, asi que la corrida se registro como PASSED sin ejecutar una sola prueba.
#
# El dialecto que ctest si entiende es el basico: `^ $ . [ ] ( ) | * + ?` y `\` como escape. Ni
# grupos no capturantes, ni `{n,m}`, ni clases como `\d`. De ahi las dos diferencias con
# `re.escape`: el grupo se abre con `(` a secas, y solo se escapan los metacaracteres de ESE motor
# Los guiones de nombres CTest ordinarios no son metacaracteres y no deben escaparse.
CTEST_REGEX_SPECIALS = frozenset("\\^$.[]()|*+?")


def ctest_name_filter(names: list[str]) -> str:
    """Filtro `-R` que casa EXACTAMENTE los nombres dados, en el dialecto de ctest."""
    alternatives = [
        "".join("\\" + character if character in CTEST_REGEX_SPECIALS else character
                for character in name)
        for name in names
    ]
    return "^(" + "|".join(alternatives) + ")$"


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
        self.ctest_ready_linkers: set[str] = set()
        self.bootstrap_lock_path: Path | None = None

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
        channel: str = "",
        optimization: str = "General",
    ) -> None:
        line = text.rstrip("\r\n")
        if not line:
            return
        stamped = f"[{datetime.now().strftime('%H:%M:%S')}] {line}"
        channels = (
            [channel]
            if channel
            else (
                ["Compilador"]
                if self.plan.operation in ("compiler", "install")
                else self.plan.linkers
            )
        )
        if not channels:
            channels = ["Compilador"]
        with self.console_lock:
            if self.console_path is not None:
                with self.console_path.open("a", encoding="utf-8", errors="replace") as handle:
                    handle.write(stamped + "\n")
            if self.run_dir is not None:
                for current_channel in channels:
                    channel_path = (
                        self.run_dir
                        / safe_name(current_channel)
                        / safe_name(optimization)
                        / "console.log"
                    )
                    channel_path.parent.mkdir(parents=True, exist_ok=True)
                    with channel_path.open("a", encoding="utf-8", errors="replace") as handle:
                        handle.write(stamped + "\n")
        for current_channel in channels:
            self.output.emit(stamped, level, current_channel, optimization)

    def _report_failure(self, failure: dict) -> None:
        row = dict(failure)
        row.setdefault("linker", "Compilador")
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
        channel: str = "",
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
        env.pop("NSC_LLVM_LIB", None)
        prefix = Path(self.plan.prefix)
        temp_root = Path(self.plan.temp_root).resolve()
        temp_root.mkdir(parents=True, exist_ok=True)
        env["TEMP"] = str(temp_root)
        env["TMP"] = str(temp_root)
        env["TMPDIR"] = str(temp_root)
        env["NS_ROOT"] = str(prefix)
        env["NSC_EXE"] = str(prefix / "bin" / "nsc.exe")
        env["NS_LIB_ROOT"] = str(prefix / "lib")
        env["NS_LIB_READY"] = "1"
        runtime_library = (
            Path(self.plan.compiler_build_dir).resolve()
            / "generated"
            / "NativeSharp.Runtime.Win32.lib"
        )
        if runtime_library.is_file():
            env["NSC_WINDOWS_RUNTIME_LIBRARY"] = str(runtime_library)
        # Both layers are intentional.  CTest gets the documented non-interactive setting and
        # TestContext installs the same protection inside every native test process before it can
        # launch generated executables through std::system().  Failures still return their real
        # exit status and remain visible in the logs; only modal UI is forbidden.
        env["CTEST_INTERACTIVE_DEBUG_MODE"] = "0"
        env["NSC_TEST_NONINTERACTIVE"] = "1"
        # Las pruebas negativas ya conservan stdout/stderr y evidencia en el historial de esta
        # corrida. Evita que cada invocación esperadamente fallida vuelva a escribir error.log,
        # error1.log, etc. El comportamiento normal de nsc fuera del builder no cambia.
        env["NSC_NO_ERROR_LOG"] = "1"
        if linker == "lld-link":
            env["NSC_LLD_LINK"] = str(Path(self.plan.llvm_root) / "bin" / "lld-link.exe")
        elif linker == "NSLink":
            env["NSC_LLD_LINK"] = str(self._effective_nslink_executable())
        # Enlazador y librarian son herramientas distintas. NSLink construye PE, pero no debe
        # recibir /lib ni archivos archive como si fueran objetos COFF.
        env["NSC_LLVM_LIB"] = str(Path(self.plan.llvm_root) / "bin" / "llvm-lib.exe")
        if self.run_dir is not None:
            linker_name = safe_name(linker or "build")
            env["NSC_LOG_DIR"] = str(self.run_dir / linker_name)
            env["NSC_TEST_EVIDENCE_DIR"] = str(self.run_dir / linker_name / "evidence")
            if self.plan.keep_link_temps and linker is not None:
                env["NSC_KEEP_LINK_TEMPS"] = str(self.run_dir / linker_name / "linktemps")
        return env

    def _effective_nslink_executable(self) -> Path:
        if self.plan.operation == "baseline":
            return Path(self.plan.prefix).resolve() / "bin" / "nslink.exe"
        return Path(self.plan.nslink_exe).resolve()

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
                channels = (
                    [linker_match.group(1)]
                    if linker_match
                    else (
                        ["Compilador"]
                        if self.plan.operation in ("compiler", "install")
                        else self.plan.linkers
                    )
                )
                for channel in channels:
                    self._report_failure(
                        {
                            "linker": channel,
                            "optimization": "General",
                            "kind": (
                                "Compilador"
                                if self.plan.operation in ("compiler", "install")
                                else "Infraestructura de pruebas"
                            ),
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
        channel: str = "",
    ) -> bool:
        assert self.run_dir is not None
        self.stage_started.emit(name)
        log = self.run_dir / "stages" / f"{safe_name(name)}.log"
        result = self._run_process(
            command,
            Path(self.plan.root),
            env,
            log,
            timeout=timeout,
            channel=channel,
        )
        if result.cancelled:
            status = "cancelled"
        else:
            status = "passed" if result.exit_code == 0 else "failed"
        self._record_stage(name, status, result.seconds, result.exit_code, command_text(command), log)
        return status == "passed"

    def _advance(self, count: int = 1) -> None:
        self.completed_units += count
        self.progress.emit(self.completed_units, self.total_units)

    def _runs_test_stages(self) -> bool:
        """Las etapas de prueba las comparten las dos pestañas de pruebas y `baseline`.

        El Gate 0 necesita construir, instalar Y ejecutar la matriz en UNA sola corrida: los hashes
        que toma antes y despues tienen que abarcar todo, o no demuestran que nada se movio.
        """
        return self.plan.operation in ("unit", "harness", "language", "baseline")

    def _validate_paths(self) -> list[str]:
        required: list[Path] = []
        problems: list[str] = []
        building = self.plan.run_build and self.plan.build_mode != "skip"
        installing = self.plan.operation == "install"
        building_tests = self.plan.build_test_trees
        running_tests = self.plan.run_ctest or self.plan.run_cases or self.plan.run_triage
        if building or building_tests:
            required.extend(
                [
                    Path(self.plan.llvm_root) / "bin" / "clang-cl.exe",
                    Path(self.plan.llvm_root) / "bin" / "llvm-mt.exe",
                    Path(self.plan.llvm_root) / "bin" / "llvm-rc.exe",
                    Path(self.plan.llvm_root) / "bin" / "lld-link.exe",
                    Path(self.plan.llvm_root) / "bin" / "llvm-readobj.exe",
                    Path(self.plan.llvm_root) / "lib" / "LLVMCore.lib",
                    Path(self.plan.llvm_root) / "lib" / "LLVMSupport.lib",
                    Path(self.plan.llvm_root) / "lib" / "cmake" / "llvm" / "LLVMConfig.cmake",
                ]
            )
            if shutil.which("cmake") is None:
                required.append(Path("cmake.exe (PATH)"))
            if self.plan.build_runner == "ninja" and shutil.which("ninja") is None:
                required.append(Path("ninja.exe (PATH)"))
        elif installing and shutil.which("cmake") is None:
            required.append(Path("cmake.exe (PATH)"))
        if running_tests:
            # En `baseline` el compilador instalado lo produce esta misma corrida, asi que exigirlo
            # de entrada rechazaria una maquina limpia por no tener todavia lo que va a construir.
            if self.plan.operation != "baseline":
                required.append(Path(self.plan.prefix) / "bin" / "nsc.exe")
            if "lld-link" in self.plan.linkers:
                required.append(Path(self.plan.llvm_root) / "bin" / "lld-link.exe")
            if "NSLink" in self.plan.linkers and self.plan.operation != "baseline":
                required.append(Path(self.plan.nslink_exe))
        if self.plan.operation == "baseline":
            try:
                validate_baseline_nslink_source(
                    Path(self.plan.nslink_exe), Path(self.plan.prefix)
                )
            except (OSError, ValueError) as exc:
                problems.append(str(exc))
        return [str(path) for path in required if not path.is_file()] + problems

    def _test_build_dir(self, linker: str) -> Path:
        configured = self.plan.test_build_dirs.get(linker)
        if not configured:
            raise ValueError(f"No hay directorio de CTest configurado para {linker}")
        return Path(configured)

    def _configure_command(
        self,
        build_tests: bool,
        build_dir: str | Path | None = None,
    ) -> list[str]:
        llvm = Path(self.plan.llvm_root).as_posix()
        prefix = Path(self.plan.prefix).as_posix()
        selected_build = str(
            build_dir if build_dir is not None else self.plan.compiler_build_dir
        )
        command = [
            "cmake",
            "-S",
            self.plan.root,
            "-B",
            selected_build,
            "-G",
            "Ninja",
            "-DCMAKE_BUILD_TYPE=Release",
            f"-DCMAKE_INSTALL_PREFIX={prefix}",
            f"-DCMAKE_C_COMPILER={llvm}/bin/clang-cl.exe",
            f"-DCMAKE_CXX_COMPILER={llvm}/bin/clang-cl.exe",
            f"-DCMAKE_LINKER={llvm}/bin/lld-link.exe",
            f"-DCMAKE_MT={llvm}/bin/llvm-mt.exe",
            f"-DCMAKE_RC_COMPILER={llvm}/bin/llvm-rc.exe",
            "-DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld",
            "-DCMAKE_DISABLE_FIND_PACKAGE_LLVM=FALSE",
            "-UNSC_LLVM_MONOLITHIC_LIBRARY",
            f"-DLLVM_DIR={llvm}/lib/cmake/llvm",
            f"-DNSC_BUILD_TESTS={'ON' if build_tests else 'OFF'}",
            "-DNSC_WARNINGS_AS_ERRORS=ON",
            f"-DNSC_HOST_LINKAGE={self.plan.host_linkage}",
            f"-DNSC_HOST_OPTIMIZATION={self.plan.host_optimization}",
        ]
        # Always write the cache entry, including the empty value. Otherwise a build directory
        # previously used for a reproducible baseline silently keeps its fixed identity when it
        # is later reused for a distributable build, and two different binaries report the same
        # --version. A fixed id is deliberate only when the current plan explicitly requests it.
        command.append(f"-DNSC_BUILD_ID_OVERRIDE={self.plan.build_id_override}")
        return command

    def _configure_tests_command(self, linker: str) -> list[str]:
        command = self._configure_command(
            build_tests=True,
            build_dir=self._test_build_dir(linker),
        )
        compiler = (Path(self.plan.prefix) / "bin" / "nsc.exe").as_posix()
        command.append(f"-DNSC_TEST_COMPILER={compiler}")
        if self.bootstrap_lock_path is not None:
            command.append(f"-DNSC_BOOTSTRAP_LOCK={self.bootstrap_lock_path.as_posix()}")
        return command

    def _freeze_bootstrap_lock(self) -> bool:
        """Crea el lock inmutable de esta corrida sin modificar el árbol de fuentes."""
        if self.plan.operation != "baseline":
            return True
        assert self.run_dir is not None
        compiler = Path(self.plan.prefix) / "bin" / "nsc.exe"
        nslink = self._effective_nslink_executable()
        template = Path(self.plan.root) / "selfhost" / "bootstrap" / "bootstrap.lock"
        output = self.run_dir / "bootstrap" / "bootstrap.lock"
        try:
            value = write_run_bootstrap_lock(
                template,
                output,
                {"nsc0": compiler, "nslink": nslink},
            )
        except (OSError, ValueError, json.JSONDecodeError) as exc:
            self._emit(f"No se pudo congelar el bootstrap de la corrida: {exc}", "error")
            self.failed += 1
            self._report_failure(
                {
                    "linker": "Compilador",
                    "optimization": "General",
                    "kind": "Infraestructura de pruebas",
                    "source": "Bootstrap lock por corrida",
                    "detail": str(exc),
                    "error_log": str(self.console_path.resolve()) if self.console_path else "",
                }
            )
            return False
        self.bootstrap_lock_path = output.resolve()
        hashes = {
            str(item["id"]): str(item["sha256"])
            for item in value["artifacts"]
            if isinstance(item, dict) and item.get("id") in ("nsc0", "nslink")
        }
        self.metadata["bootstrap_lock"] = {
            "path": str(self.bootstrap_lock_path),
            "sha256": sha256_file(self.bootstrap_lock_path),
            "source_lock_sha256": sha256_file(template),
            "captured_artifacts": hashes,
        }
        self._save_metadata()
        self._emit(f"Bootstrap congelado para esta corrida: {self.bootstrap_lock_path}", "success")
        return True

    def _prepare_baseline_prefix(self) -> bool:
        if self.plan.operation != "baseline":
            return True
        assert self.run_dir is not None
        name = "Preparar staging del baseline"
        self.stage_started.emit(name)
        started = time.monotonic()
        log = self.run_dir / "stages" / "preparar_staging_baseline.log"
        status = "passed"
        code = 0
        detail = f"Staging limpio: {Path(self.plan.prefix).resolve()}"
        try:
            reset_baseline_prefix(Path(self.plan.prefix))
        except (OSError, ValueError) as exc:
            status = "failed"
            code = 1
            detail = f"No se pudo preparar el staging del baseline: {exc}"
            self.failed += 1
            self._emit(detail, "error")
        else:
            self._emit(detail, "success")
        log.parent.mkdir(parents=True, exist_ok=True)
        log.write_text(detail + "\n", encoding="utf-8", errors="replace")
        self._record_stage(name, status, time.monotonic() - started, code, "limpieza interna", log)
        return status == "passed"

    def _stage_baseline_nslink(self) -> bool:
        if self.plan.operation != "baseline":
            return True
        assert self.run_dir is not None
        name = "Preparar NSLink del baseline"
        self.stage_started.emit(name)
        started = time.monotonic()
        log = self.run_dir / "stages" / "preparar_nslink_baseline.log"
        status = "passed"
        code = 0
        try:
            destination = stage_baseline_nslink(
                Path(self.plan.nslink_exe), Path(self.plan.prefix)
            )
            detail = (
                f"NSLink preparado desde {Path(self.plan.nslink_exe).resolve()} hacia "
                f"{destination.resolve()}\nsha256: {sha256_file(destination)}"
            )
            self._emit(detail, "success")
        except (OSError, ValueError) as exc:
            status = "failed"
            code = 1
            detail = f"No se pudo preparar NSLink para el baseline: {exc}"
            self.failed += 1
            self._emit(detail, "error")
        log.parent.mkdir(parents=True, exist_ok=True)
        log.write_text(detail + "\n", encoding="utf-8", errors="replace")
        self._record_stage(name, status, time.monotonic() - started, code, "A: -> staging", log)
        return status == "passed"

    def _build_baseline_libraries(self) -> bool:
        if self.plan.operation != "baseline":
            return True
        prefix = Path(self.plan.prefix).resolve()
        output = prefix / "lib" / "NativeSharpCore.lib"
        output.parent.mkdir(parents=True, exist_ok=True)
        command = [
            str(prefix / "bin" / "nsc.exe"),
            "--emit-project-oop-lib",
            str(Path(self.plan.root).resolve() / "lib" / "NativeSharp.Core" /
                "NativeSharp.Core.nsproj"),
            "-O0",
            "-j",
            str(self.plan.jobs),
            "-o",
            str(output),
        ]
        return self._command_stage(
            "Construir bibliotecas bootstrap",
            command,
            self._base_environment(),
        )

    def _build(self) -> bool:
        if not self.plan.run_build:
            return True
        if self.plan.operation == "install":
            return self._command_stage(
                "Instalar",
                ["cmake", "--install", self.plan.compiler_build_dir, "--prefix", self.plan.prefix],
                self._base_environment(),
            )
        if self.plan.build_mode == "skip":
            return True
        root = Path(self.plan.root).resolve()
        build = Path(self.plan.compiler_build_dir).resolve()
        if self.plan.build_mode == "clean":
            if not build_directory_can_be_deleted(build, root, "build-"):
                detail = f"Se rechazó borrar un build fuera del destino permitido: {build}"
                self._emit(detail, "error")
                self.failed += 1
                self._report_failure(
                    {
                        "linker": "Compilador",
                        "kind": "Validación",
                        "source": "Directorio de build",
                        "detail": detail,
                        "error_log": str(self.console_path.resolve()) if self.console_path else "",
                    }
                )
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

        # Configure on every build operation. CMake keeps this incremental, while the explicit
        # cache assignments refresh the selected install root, LLVM paths and build identity.
        # Skipping configure allowed a stale NSC_BUILD_ID_OVERRIDE to survive indefinitely.
        if not self._command_stage(
            "Configurar compilador",
            self._configure_command(build_tests=False),
            self._base_environment(),
        ):
            return False
        mark_managed_build(build)

        targets = [] if self.plan.build_target == "all" else [
            target for target in dict.fromkeys(self.plan.build_target.split(";")) if target
        ]
        if self.plan.build_runner == "ninja":
            command = [
                "ninja",
                "-C",
                self.plan.compiler_build_dir,
                "-j",
                str(self.plan.jobs),
                "-k",
                "0",
                *targets,
            ]
        else:
            command = ["cmake", "--build", self.plan.compiler_build_dir]
            if targets:
                command.extend(["--target", *targets])
            command.extend(["--parallel", str(self.plan.jobs), "--", "-k", "0"])
        if not self._command_stage("Construir", command, self._base_environment()):
            return False
        if not self._prepare_baseline_prefix():
            return False
        if not self._command_stage(
            "Instalar",
            ["cmake", "--install", self.plan.compiler_build_dir, "--prefix", self.plan.prefix],
            self._base_environment(),
        ):
            return False
        if not self._stage_baseline_nslink():
            return False
        return self._build_baseline_libraries()

    def _build_tests(self) -> bool:
        if not self.plan.build_test_trees:
            # NADA QUE CONSTRUIR AQUI, y es el caso normal de la pestana del lenguaje. Los arboles
            # que ya existan sirven tal cual; el que falte se dira cuando ctest no lo encuentre, no
            # construyendolo por sorpresa.
            self.ctest_ready_linkers = set(self.plan.linkers)
            return True
        root = Path(self.plan.root).resolve()
        self.ctest_ready_linkers.clear()

        # Al construir desde la pestana del compilador el plan no lleva enlazadores --no se va a
        # ejecutar nada-- pero los dos arboles hay que dejarlos listos igual.
        for linker in (self.plan.linkers or list(self.plan.test_build_dirs)):
            if self.cancel_event.is_set():
                return False
            build = self._test_build_dir(linker).resolve()
            if not build_directory_allowed(
                build,
                root,
                "build-test-",
                self.plan.allowed_write_roots,
            ):
                detail = (
                    "Se rechazó usar el árbol de CTest porque no es un build-test-* directo "
                    f"de {root} ni está dentro de una raíz autorizada: {build}"
                )
                self._emit(detail, "error", linker)
                self.failed += 1
                self._report_failure(
                    {
                        "linker": linker,
                        "kind": "Validación",
                        "source": f"Directorio CTest [{linker}]",
                        "detail": detail,
                        "error_log": str(self.console_path.resolve()) if self.console_path else "",
                    }
                )
                continue

            # «NO CONSTRUIR»: la pestana de Pruebas no reconstruye nada, usa lo que ya hay.
            #
            # POR QUE ES EL MODO POR DEFECTO. Construir y probar son dos decisiones distintas y no
            # tiene sentido encadenarlas: si el compilador no construye, no hay nada que probar, y
            # correr pruebas contra un nsc.exe que se acaba de tocar no responde la pregunta que se
            # estaba haciendo. Se construye en la pestana de construccion, a proposito, y despues se
            # prueba lo construido.
            #
            # Y HAY UN MOTIVO CONCRETO, no solo de orden. `nsc_build_identity` esta SIEMPRE
            # desactualizado a proposito (CMakeLists.txt:12-16, CC1-000): reescribe BuildId.h con
            # marca de tiempo y numero aleatorio en CADA construccion. `nsc_driver` depende de el y
            # 56 targets enlazan `nsc_driver`, asi que cualquier construccion en este arbol
            # recompila Version.cpp, re-archiva nsc_driver y re-enlaza todo lo que cuelgue de ahi
            # --tambien cuando lo unico que cambio fue una prueba, o nada--. Medido: 66 s para no
            # ejecutar ni una sola prueba.
            if self.plan.test_build_mode == "skip":
                if not (build / "CMakeCache.txt").is_file():
                    detail = (
                        f"«No construir» necesita un árbol de CTest ya configurado y {build} no lo "
                        "está. Constrúyelo una vez y vuelve a intentarlo."
                    )
                    self._emit(detail, "error", linker)
                    self.failed += 1
                    self._report_failure(
                        {
                            "linker": linker,
                            "kind": "Validación",
                            "source": f"Directorio CTest [{linker}]",
                            "detail": detail,
                            "error_log": str(self.console_path.resolve())
                            if self.console_path
                            else "",
                        }
                    )
                    continue
                self._emit(
                    f"CTest [{linker}]: se usa el árbol ya construido, sin reconstruir nada.",
                    "info",
                    linker,
                )
                self.ctest_ready_linkers.add(linker)
                continue

            if self.plan.test_build_mode == "clean":
                name = f"Limpiar CTest [{linker}]"
                self.stage_started.emit(name)
                started = time.monotonic()
                status = "passed"
                code = 0
                detail = f"Árbol CTest limpio para {linker}: {build}"
                try:
                    if not build_directory_can_be_deleted(build, root, "build-test-"):
                        raise ValueError(
                            "se rechazó borrar un build configurable sin marcador administrado: "
                            f"{build}"
                        )
                    if build.exists():
                        shutil.rmtree(build)
                except OSError as exc:
                    status = "failed"
                    code = 1
                    detail = f"No se pudo limpiar {build}: {exc}"
                self._emit(detail, "info" if status == "passed" else "error", linker)
                assert self.run_dir is not None
                log = self.run_dir / "stages" / f"{safe_name(name)}.log"
                log.parent.mkdir(parents=True, exist_ok=True)
                log.write_text(detail + "\n", encoding="utf-8", errors="replace")
                self._record_stage(
                    name,
                    status,
                    time.monotonic() - started,
                    code,
                    "limpieza interna",
                    log,
                )
                if status != "passed":
                    continue

            # Reconfigure every buildable CTest tree so a changed prefix, compiler path or build
            # identity cannot remain hidden in an old cache. This does not rebuild targets that
            # Ninja already considers current.
            if not self._command_stage(
                f"Configurar CTest [{linker}]",
                self._configure_tests_command(linker),
                self._base_environment(linker),
                channel=linker,
            ):
                continue
            mark_managed_build(build)

            targets = [
                target for target in dict.fromkeys(self.plan.test_build_target.split(";")) if target
            ]
            if not targets:
                # La seleccion no necesita ningun ejecutable de prueba --por ejemplo `nsc.version`,
                # que corre el compilador instalado--. El arbol ya quedo configurado arriba, que es
                # lo unico que ctest necesita para conocerlas.
                self._emit(
                    f"CTest [{linker}]: la selección no requiere construir ningún ejecutable de "
                    "prueba.",
                    "info",
                    linker,
                )
                self.ctest_ready_linkers.add(linker)
                continue
            if self.plan.build_runner == "ninja":
                command = [
                    "ninja",
                    "-C",
                    str(build),
                    "-j",
                    str(self.plan.jobs),
                    "-k",
                    "0",
                    *targets,
                ]
            else:
                command = ["cmake", "--build", str(build), "--target", *targets]
                command.extend(["--parallel", str(self.plan.jobs), "--", "-k", "0"])
            if self._command_stage(
                f"Construir CTest [{linker}]",
                command,
                self._base_environment(linker),
                channel=linker,
            ):
                self.ctest_ready_linkers.add(linker)

        return not self.cancel_event.is_set()

    def _sync_libraries(self) -> bool:
        if not self.plan.sync_libraries:
            return True
        assert self.run_dir is not None
        name = "Validar bibliotecas locales"
        self.stage_started.emit(name)
        started = time.monotonic()
        log = self.run_dir / "stages" / "validar_bibliotecas_locales.log"
        source_root = Path(self.plan.root).resolve() / "lib"
        libraries = ("NativeSharp.Core", "NativeSharp.Core.Collections.Generic")
        lines = [f"origen_local={source_root}", "copia=ninguna"]
        status = "passed"
        code = 0
        try:
            for library in libraries:
                source = source_root / library
                project = source / f"{library}.nsproj"
                if not project.is_file():
                    raise FileNotFoundError(f"Falta la biblioteca local: {project}")
                lines.append(f"validada={library}")
                self._emit(f"[LIB] Validada {library} en {source}")
        except OSError as exc:
            status = "failed"
            code = 1
            lines.append(f"error={exc}")
            self._emit(f"[LIB] {exc}", "error")
        elapsed = time.monotonic() - started
        log.parent.mkdir(parents=True, exist_ok=True)
        log.write_text("\n".join(lines) + "\n", encoding="utf-8", errors="replace")
        self._record_stage(name, status, elapsed, code, "sincronización interna", log)
        return status == "passed"

    def _prepare_test_outputs(self) -> bool:
        """Prepara el área persistente donde nsc genera los programas de prueba.

        El modo limpio elimina únicamente los artefactos de los enlazadores elegidos. El modo
        incremental conserva objetos y ejecutables anteriores y vuelve a generar sólo los casos
        seleccionados por esta corrida.
        """
        if (
            not self._runs_test_stages()
            or self.plan.test_build_mode != "clean"
            or not (self.plan.run_cases or self.plan.run_triage)
        ):
            return True
        assert self.run_dir is not None
        root = Path(self.plan.test_work_root).resolve()
        for linker in self.plan.linkers:
            name = f"Limpiar pruebas [{linker}]"
            self.stage_started.emit(name)
            started = time.monotonic()
            target = (root / safe_name(linker)).resolve()
            log = self.run_dir / "stages" / f"{safe_name(name)}.log"
            status = "passed"
            code = 0
            try:
                if target.parent != root:
                    raise ValueError(f"Destino de pruebas fuera del área permitida: {target}")
                if target.exists():
                    shutil.rmtree(target)
                target.mkdir(parents=True, exist_ok=True)
                detail = f"Área limpia: {target}"
                self._emit(detail, "info", linker)
            except (OSError, ValueError) as exc:
                status = "failed"
                code = 1
                detail = f"No se pudo limpiar el área de pruebas: {exc}"
                self._emit(detail, "error", linker)
            log.parent.mkdir(parents=True, exist_ok=True)
            log.write_text(detail + "\n", encoding="utf-8", errors="replace")
            self._record_stage(name, status, time.monotonic() - started, code, "limpieza interna", log)
            if status != "passed":
                return False
        return True

    def _ctest(self, linker: str) -> None:
        if not self.plan.run_ctest or self.cancel_event.is_set():
            return
        if linker not in self.ctest_ready_linkers:
            self._emit(
                f"CTest [{linker}] omitido porque su árbol de pruebas no pudo construirse.",
                "error",
                linker,
            )
            return
        assert self.run_dir is not None
        name = f"CTest [{linker}]"
        command = [
            "ctest",
            "--test-dir",
            str(self._test_build_dir(linker)),
            # CTest defaults to interactive crash handling on Windows.  Set this explicitly even
            # though the builder process also installs SEM_NOGPFAULTERRORBOX: CTest can otherwise
            # let Windows Error Reporting display a modal dialog for a crashing test and stall the
            # unattended baseline until somebody dismisses it.
            "--interactive-debug-mode",
            "0",
            "--output-on-failure",
            # Una corrida que no ejecuta ninguna prueba no es una corrida verde. Sin esto ctest
            # sale con 0 cuando el filtro no casa nada, que es como «No tests were found!!!» acabo
            # registrandose como PASSED. Requiere CMake 3.18 o posterior.
            "--no-tests=error",
            "-j",
            str(self.plan.jobs),
        ]
        if self.plan.selected_ctests:
            command.extend(["-R", ctest_name_filter(self.plan.selected_ctests)])
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

        # CASOS SIN EJECUTAR. La regla 13 del plan los cuenta igual que un fallo --«ni casos sin
        # ejecutar»-- y hasta aqui no se veian: el estado salia solo del codigo de salida.
        #
        # `--no-tests=error` cubre el caso de CERO. Esto cubre el PARCIAL, que es el mismo fallo y
        # se ve peor: pedir cinco pruebas por nombre, que solo tres existan --un nombre mal escrito,
        # una prueba renombrada, un `add_test` que se cayo del CMakeLists-- y que las tres verdes
        # tapen a las dos que no llegaron a existir. ctest sale con 0 porque, de lo que corrio, no
        # fallo nada; la pregunta que importa es otra: corrio lo que se pidio.
        #
        # Solo se comprueba con `selected_ctests`, que es el unico caso donde se sabe de antemano
        # cuantas debian correr.
        executed = 0
        shortfall = 0
        if self.plan.selected_ctests:
            counter = re.search(r"out of\s+(\d+)", result.output)
            executed = int(counter.group(1)) if counter is not None else 0
            shortfall = len(self.plan.selected_ctests) - executed
            if shortfall != 0 and status == "passed":
                status = "failed"

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
        if shortfall != 0:
            detail = (
                f"ctest ejecuto {executed} de las {len(self.plan.selected_ctests)} pruebas pedidas"
            )
            self._emit(f"{name}: {detail}.", "error", linker)
            self._report_failure(
                {
                    "linker": linker,
                    "optimization": "General",
                    "kind": "CTest",
                    "source": name,
                    "detail": detail,
                    "error_log": str(log.resolve()),
                }
            )
            if result.exit_code == 0:
                return
        failure_status = r"(?:Failed|Timeout|Not Run|Exception|SEGFAULT|BAD_COMMAND)"
        failed_names = re.findall(
            rf"^\s*\d+\s+-\s+([^\s]+)\s+\({failure_status}\)\s*$",
            result.output,
            re.MULTILINE | re.IGNORECASE,
        )
        if not failed_names:
            failed_names = re.findall(
                rf"Test\s+#\d+:\s+([^\s]+).*?\*{{3}}\s*{failure_status}",
                result.output,
                re.IGNORECASE,
            )
        if not failed_names and len(self.plan.selected_ctests) == 1:
            failed_names = list(self.plan.selected_ctests)
        for failed_name in dict.fromkeys(failed_names or [name]):
            self._report_failure(
                {
                    "linker": linker,
                    "optimization": "General",
                    "kind": "CTest",
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
        artifact_dir = (
            Path(self.plan.test_work_root)
            / safe_name(linker)
            / optimization.lstrip("-").upper()
            / safe_name(case.label)
        )
        exe = artifact_dir / f"{safe_name(case.label)}.exe"
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
            compiler = Path(self.plan.prefix) / "bin" / "nsc.exe"
            linker_exe = Path(env.get("NSC_LLD_LINK", ""))
            fingerprint = incremental_fingerprint(project, compiler, linker_exe)
            manifest = artifact_dir / "incremental.json"
            obj = exe.with_suffix(".obj")
            cached = False
            if self.plan.test_build_mode == "incremental" and obj.is_file() and exe.is_file():
                try:
                    cache = json.loads(manifest.read_text(encoding="utf-8"))
                    cached = cache.get("fingerprint") == fingerprint
                except (OSError, ValueError, TypeError):
                    cached = False
            if cached:
                self._emit(
                    f"{prefix} INCREMENTAL — sin cambios; se reutiliza {exe}",
                    "success",
                    linker,
                    optimization.lstrip("-"),
                )
                executable_ready = True
            else:
                try:
                    manifest.unlink(missing_ok=True)
                except OSError:
                    pass
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
                    executable_ready = False
                elif object_build.exit_code != 0:
                    status, detail = "failed", object_build.error or "falló la emisión del objeto"
                    error_log = log.with_suffix(".obj.log")
                    executable_ready = False
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
                        executable_ready = False
                    elif executable_build.exit_code != 0:
                        status, detail = (
                            "failed",
                            executable_build.error or "falló la emisión o el enlace del ejecutable",
                        )
                        error_log = log.with_suffix(".exe.log")
                        executable_ready = False
                    else:
                        write_json(
                            manifest,
                            {
                                "fingerprint": fingerprint,
                                "project": str(project.resolve()),
                                "compiler": str(compiler.resolve()),
                                "linker": str(linker_exe.resolve()),
                                "optimization": optimization,
                                "built_at": utc_stamp(),
                            },
                        )
                        executable_ready = True
            if executable_ready:
                if case.expected is None:
                    status = "passed"
                    detail = "sin cambios" if cached else "compila y enlaza"
                else:
                    run = self._run_process(
                        [str(exe)],
                        # Cada variante tiene su propio directorio de artefactos. Ejecutarla ahi
                        # tambien aisla los archivos relativos que cree el fixture: si O1 y O2
                        # comparten la raiz del repositorio pueden truncarse o borrarse entre si.
                        artifact_dir,
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
                        suffix = "; binario incremental" if cached else ""
                        status, detail = "passed", f"exit {actual}{suffix}"
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
                            "kind": "Fixtures y ejemplos",
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
        work = [(case, optimization) for case in cases for optimization in self.plan.optimizations]
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
                            "kind": "Bug triage",
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

    # ------------------------------------------------------------------ Gate 0 del plan 123

    def _abi_measure(self, moment: str, *, sources: bool = True, binaries: bool = True) -> dict:
        """La foto del arbol y de los binarios en un instante.

        Se toma TRES veces, y el orden lo fija G0.2 del plan 123: `S0` antes de construir --si se
        tomara despues incluiria lo que la propia construccion toque--, `B0`/`I0` despues de
        construir e instalar TANTO el compilador COMO los dos arboles de pruebas, justo antes de
        ejecutar la matriz, y todo otra vez al terminar. La foto intermedia no puede adelantarse a
        `_build_tests()`: esa etapa crea/re-enlaza precisamente los `nsc_*tests.exe` que esta
        medicion inventaria y haria que el gate se invalidara a si mismo. Sin la ultima foto, unos
        logs verdes pueden pertenecer a un binario o a unas fuentes que ya no son las que se
        archivan, que es exactamente el fallo que el Gate 0 existe para impedir.
        """
        root = Path(self.plan.root).resolve()
        prefix = Path(self.plan.prefix).resolve()
        build = Path(self.plan.compiler_build_dir).resolve() if self.plan.compiler_build_dir else None

        measurement: dict = {"moment": moment, "binaries": {}, "tools": {}}
        if sources:
            manifest = canonical_manifest(root, collect_source_files(root))
            measurement["source_digest"] = manifest_digest(manifest)
            measurement["source_file_count"] = len(manifest)
        if not binaries:
            return measurement
        # B: el compilador CONSTRUIDO. I: el INSTALADO. Son dos ficheros distintos y las fixtures
        # usan el instalado, asi que hay que probar que son el mismo antes de certificar nada.
        built = (build / "bin" / "nsc.exe") if build is not None else None
        if built is not None and built.is_file():
            measurement["binaries"]["built_nsc"] = sha256_file(built)
        installed = prefix / "bin" / "nsc.exe"
        if installed.is_file():
            measurement["binaries"]["installed_nsc"] = sha256_file(installed)
        for label, path in (
            ("lld-link", Path(self.plan.llvm_root) / "bin" / "lld-link.exe"),
            ("nslink", self._effective_nslink_executable()),
        ):
            if path.is_file():
                measurement["tools"][label] = sha256_file(path)
        for linker in self.plan.linkers:
            try:
                test_build = self._test_build_dir(linker).resolve()
            except ValueError:
                continue
            for executable in sorted(test_build.rglob("nsc_*tests.exe")):
                key = f"{linker}/{executable.name}"
                measurement["tools"][key] = sha256_file(executable)
        return measurement

    def _abi_compare(self, s0: dict, built: dict, final: dict) -> list[str]:
        """Las cinco igualdades que el plan 123 exige en G0.2, una a una y con su nombre."""
        problems: list[str] = []
        if s0.get("source_digest") != final.get("source_digest"):
            problems.append(
                "S0 != S1: el arbol de fuentes cambio durante la corrida "
                f"({str(s0.get('source_digest'))[:16]} -> {str(final.get('source_digest'))[:16]})"
            )
        b0 = built["binaries"].get("built_nsc")
        i0 = built["binaries"].get("installed_nsc")
        b1 = final["binaries"].get("built_nsc")
        i1 = final["binaries"].get("installed_nsc")
        if b0 is None or b1 is None:
            problems.append("No se pudo hashear el nsc.exe construido antes y despues de la matriz")
        elif b0 != b1:
            problems.append("B0 != B1: el nsc.exe construido cambio durante la matriz")
        if i0 is None or i1 is None:
            problems.append("No se pudo hashear el nsc.exe instalado antes y despues de la matriz")
        elif i0 != i1:
            problems.append("I0 != I1: el nsc.exe instalado cambio durante la matriz")
        if b0 is not None and i0 is not None and b0 != i0:
            problems.append(
                "B0 != I0: lo instalado no es lo que se acaba de construir"
            )
        if b1 is not None and i1 is not None and b1 != i1:
            problems.append(
                "B1 != I1: el nsc.exe construido y el instalado no son el mismo fichero, "
                "y las fixtures usan el instalado"
            )
        for key, value in built["tools"].items():
            if final["tools"].get(key) != value:
                problems.append(
                    f"La herramienta o ejecutable de prueba '{key}' cambio durante la matriz"
                )
        for key in final["tools"]:
            if key not in built["tools"]:
                problems.append(f"Aparecio '{key}' durante la matriz y no estaba al empezar")
        return problems

    def _abi_gate_state(self, state: str, payload: dict) -> Path:
        gate_dir = ABI_GATES_ROOT / "G00"
        gate_dir.mkdir(parents=True, exist_ok=True)
        record = dict(payload)
        record["state"] = state
        record["recorded_at"] = utc_stamp()
        write_json(gate_dir / "gate.json", record)
        return gate_dir

    def _abi_baseline(self, s0: dict, built: dict | None) -> None:
        """Cierra el Gate 0: ultima medicion, comparacion, y estado en `_abi/gates/G00`."""
        assert self.run_dir is not None
        name = "Certificar baseline (Gate 0)"
        self.stage_started.emit(name)
        started = time.monotonic()
        log = self.run_dir / "stages" / "gate0_baseline.log"
        log.parent.mkdir(parents=True, exist_ok=True)
        lines: list[str] = []
        try:
            if built is None:
                built = {"moment": "after-build", "binaries": {}, "tools": {}}
            after = self._abi_measure("after-run")
            problems = self._abi_compare(s0, built, after)
            payload = {
                "gate": "G00",
                "run_id": self.metadata.get("id"),
                "run_dir": str(self.run_dir.resolve()),
                "root": self.plan.root,
                "prefix": self.plan.prefix,
                "deploy_prefix": self.plan.deploy_prefix,
                "llvm_root": self.plan.llvm_root,
                "compiler_build_dir": self.plan.compiler_build_dir,
                "linkers": list(self.plan.linkers),
                "optimizations": list(self.plan.optimizations),
                "s0_sources": s0,
                "after_build": built,
                "after_run": after,
                "results": {
                    "failed": self.failed,
                    "stages": [
                        {
                            "name": row.get("name"),
                            "status": row.get("status"),
                            "exit_code": row.get("exit_code"),
                        }
                        for row in self.stage_rows
                    ],
                    "cases": [
                        {
                            "label": row.get("label"),
                            "linker": row.get("linker"),
                            "optimization": row.get("optimization"),
                            "status": row.get("status"),
                            "expected": row.get("expected"),
                            "actual": row.get("actual"),
                        }
                        for row in self.case_rows
                    ],
                },
                "problems": problems,
            }
            # DOS CONDICIONES DISTINTAS, Y NO SE MEZCLAN.
            #
            # CERTIFICAR EL BASELINE es lo que pide el paso 10 de G0.2, y lo que el Cierre G0
            # exige para ello es que el arbol permaneciera inmovil durante la medicion: eso es
            # `problems`, las cinco igualdades del paso 9. Una corrida con pruebas en rojo mide la
            # inmovilidad exactamente igual de bien --el hash de un binario no deja de ser cierto
            # porque un caso falle-- asi que el baseline se calcula y se guarda igual.
            #
            # ATERRIZAR EL GATE es otra cosa. Regla 13: «Un gate no se marca `LANDED` mientras
            # conserve fallos, pendientes obligatorios o casos sin ejecutar», y la §2 define
            # aterrizar como «un gate termino verde». Por eso `self.failed` decide el ESTADO y no
            # la certificacion.
            #
            # Separarlas importa por lo que pasa cuando difieren: un G0 en `FAILED` por un fallo
            # ajeno al gate --Gate 0 no modifica el compilador-- conserva aun asi el baseline
            # medido, que es justamente la red de seguridad para la que existe el gate. Fundirlas
            # en una sola condicion tiraria la medicion buena junto con el fallo.
            certified = not problems
            promotion: dict[str, str] = {"status": "not-run"}
            if certified and self.failed == 0:
                try:
                    promoted = promote_certified_baseline(
                        Path(self.plan.prefix),
                        Path(self.plan.deploy_prefix),
                        str(self.metadata.get("id", "")),
                    )
                    promotion = {"status": "passed", **promoted}
                    self._emit(
                        "Baseline certificado promovido: "
                        f"{promoted['source']} -> {promoted['destination']}",
                        "success",
                    )
                except (OSError, ValueError) as exc:
                    self.failed += 1
                    promotion = {"status": "failed", "error": str(exc)}
                    self._emit(f"No se pudo promover el baseline certificado: {exc}", "error")
            payload["promotion"] = promotion
            state = "LANDED" if certified and self.failed == 0 else "FAILED"
            payload["certified"] = certified
            gate_dir = self._abi_gate_state(state, payload)
            lines.append(f"estado: {state}")
            lines.append(
                "baseline certificado: " + ("si" if certified else "no (el arbol se movio)"))
            lines.append(f"fallos registrados en la corrida: {self.failed}")
            if certified and self.failed:
                lines.append(
                    f"NO LANDED por la regla 13: el arbol permanecio inmovil y el baseline quedo "
                    f"certificado, pero la corrida conserva {self.failed} fallo(s)")
            lines.append(f"S0: {s0.get('source_digest')}")
            lines.append(f"S1: {after.get('source_digest')}")
            for key, value in sorted(after["binaries"].items()):
                lines.append(f"{key}: {value}")
            for problem in problems:
                lines.append(f"problema: {problem}")
            lines.append(f"estado escrito en: {gate_dir}")
            status = "passed" if state == "LANDED" else "failed"
            code = 0 if state == "LANDED" else 1
            for problem in problems:
                self._emit(problem, "error")
            if state == "LANDED":
                self._emit(f"Gate 0 LANDED. Baseline registrado en {gate_dir}", "success")
            elif certified:
                self._emit(
                    f"Gate 0 FAILED por la regla 13: {self.failed} fallo(s) en la corrida. El "
                    f"baseline SI quedo certificado y registrado en {gate_dir}",
                    "error")
        except Exception as exc:  # pragma: no cover - depende del arbol del usuario
            status, code = "failed", -1
            lines.append(f"excepcion: {exc!r}")
            self._emit(f"Gate 0 no pudo completarse: {exc!r}", "error")
        log.write_text("\n".join(lines) + "\n", encoding="utf-8", errors="replace")
        self._record_stage(name, status, time.monotonic() - started, code, "", log)

    def _calculate_total(self) -> int:
        total = 0
        if self.plan.run_build and self.plan.operation == "install":
            total += 1
        elif self.plan.run_build and self.plan.build_mode != "skip":
            if self.plan.build_mode == "clean":
                total += 4  # limpiar, configurar, construir, instalar
            elif not (Path(self.plan.compiler_build_dir) / "CMakeCache.txt").is_file():
                total += 3  # configurar, construir, instalar
            else:
                total += 2  # construir, instalar
            if self.plan.operation == "baseline":
                total += 3  # vaciar staging, preparar NSLink y construir bibliotecas bootstrap
        if self._runs_test_stages() and self.plan.run_ctest:
            for linker in self.plan.linkers:
                build = self._test_build_dir(linker)
                # Una selección que no necesita ningún ejecutable de prueba no tiene etapa de
                # construcción: contarla dejaría la barra de progreso mintiendo.
                builds = bool([t for t in self.plan.build_target.split(";") if t])
                if self.plan.test_build_mode == "skip":
                    pass  # no se construye nada: no hay etapa que contar
                elif self.plan.test_build_mode == "clean":
                    total += 2 + int(builds)  # limpiar, configurar y (quizá) construir
                elif not (build / "CMakeCache.txt").is_file():
                    total += 1 + int(builds)  # configurar y (quizá) construir
                else:
                    total += int(builds)  # construcción incremental de SU árbol de CTest
        if self.plan.sync_libraries:
            total += 1
        if self.plan.operation == "baseline":
            total += 1  # la etapa de cierre del Gate 0
        if (
            self._runs_test_stages()
            and self.plan.test_build_mode == "clean"
            and (self.plan.run_cases or self.plan.run_triage)
        ):
            total += len(self.plan.linkers)
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
        linkers = list(dict.fromkeys(["Compilador", *self.plan.linkers]))
        optimizations = ("General", "O0", "O1", "O2", "O3")
        for linker in linkers:
            for optimization in optimizations:
                rows = [
                    row
                    for row in self.failure_rows
                    if (
                        row.get("linker")
                        if row.get("linker") in self.plan.linkers
                        else "Compilador"
                    )
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
        write_json(self.run_dir / "report.json", structured_run_report(self.metadata))
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
        run_name = self.plan.run_id or (
            datetime.now().strftime("%Y%m%d-%H%M%S") + "-" + uuid.uuid4().hex[:8]
        )
        if safe_name(run_name) != run_name:
            raise ValueError(f"Identificador de corrida inseguro: {run_name!r}")
        self.run_dir = Path(self.plan.history_root) / run_name
        self.run_dir.mkdir(parents=True, exist_ok=False)
        self.console_path = self.run_dir / "console.log"
        self.console_path.touch()
        for channel in dict.fromkeys(["Compilador", *self.plan.linkers]):
            for optimization in ("General", "O0", "O1", "O2", "O3"):
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
                    channels = (
                        ["Compilador"]
                        if self.plan.operation in ("compiler", "install")
                        else self.plan.linkers
                    )
                    for channel in channels:
                        self._report_failure(
                            {
                                "linker": channel,
                                "optimization": "General",
                                "kind": "Validación",
                                "source": "Validación de herramientas",
                                "detail": f"Falta herramienta: {path}",
                                "error_log": str(self.console_path.resolve()),
                            }
                        )
                self.failed += len(missing)
            else:
                # Gate 0. La primera medicion se toma ANTES de construir: si se tomara despues, el
                # digest de fuentes ya incluiria cualquier cosa que la propia construccion tocara.
                # Gate 0. El orden de las mediciones lo fija G0.2 y no es intercambiable: `S0`
                # antes de construir; `B0`/`I0` despues de construir/instalar el compilador Y de
                # terminar ambos arboles CTest; todo otra vez al terminar la matriz. Medir antes
                # de `_build_tests()` hace que la propia corrida cambie o cree los ejecutables
                # que `_abi_measure()` vigila y vuelve imposible certificar un baseline limpio.
                is_baseline = self.plan.operation == "baseline"
                baseline_sources = (
                    self._abi_measure("before-build", binaries=False) if is_baseline else None
                )
                baseline_built: dict | None = None
                if (
                    self._build()
                    and not self.cancel_event.is_set()
                    and self._freeze_bootstrap_lock()
                    and not self.cancel_event.is_set()
                ):
                    if (
                        self._prepare_test_outputs()
                        and not self.cancel_event.is_set()
                        and self._build_tests()
                        and not self.cancel_event.is_set()
                        and self._sync_libraries()
                        and not self.cancel_event.is_set()
                    ):
                        if is_baseline:
                            baseline_built = self._abi_measure("after-build", sources=False)
                        for linker in self.plan.linkers:
                            self._ctest(linker)
                            self._run_case_group(linker)
                            self._triage(linker)
                            if self.cancel_event.is_set():
                                break
                if baseline_sources is not None and not self.cancel_event.is_set():
                    self._abi_baseline(baseline_sources, baseline_built)
        except BaseException as exc:
            self.failed += 1
            self._emit(f"Fallo interno del runner: {exc!r}", "error")
            exception_log = self.run_dir / "runner_exception.log"
            exception_log.write_text(
                traceback.format_exc(), encoding="utf-8", errors="replace"
            )
            channels = (
                ["Compilador"]
                if self.plan.operation in ("compiler", "install")
                else self.plan.linkers
            )
            for channel in channels:
                self._report_failure(
                    {
                        "linker": channel,
                        "optimization": "General",
                        "kind": "Runner interno",
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
            try:
                write_json(
                    self.run_dir / "summary.json",
                    run_history_summary(self.metadata, self.run_dir),
                )
            except OSError as exc:
                # El reporte canónico ya quedó escrito. Un índice auxiliar no debe convertir una
                # corrida válida en fallo, pero sí quedar visible para poder reparar el disco.
                self._emit(f"No se pudo escribir el resumen del historial: {exc}", "warn")
            self._emit(f"Corrida {status.upper()} en {elapsed:.2f} s", "success" if status == "passed" else "error")
            self.finished.emit(
                {
                    "operation": self.plan.operation,
                    "status": status,
                    "seconds": elapsed,
                    "failed": self.failed,
                    "run_dir": str(self.run_dir),
                    "report": str(self.run_dir / "report.md"),
                }
            )


class CleanupWorker(QObject):
    output = Signal(str, str, str, str)
    progress = Signal(int, int)
    finished = Signal(dict)

    def __init__(self, targets: list[Path]) -> None:
        super().__init__()
        self.targets = targets
        self.cancel_event = threading.Event()

    def cancel(self) -> None:
        self.cancel_event.set()

    def _emit(self, text: str, level: str = "info") -> None:
        stamped = f"[{datetime.now().strftime('%H:%M:%S')}] {text}"
        self.output.emit(stamped, level, "Compilador", "General")

    @Slot()
    def run(self) -> None:
        started = time.monotonic()
        removed = 0
        failures: list[str] = []
        total = len(self.targets)
        self.progress.emit(0, total)
        self._emit("Limpieza de artefactos iniciada.", "command")
        for index, target in enumerate(self.targets, start=1):
            if self.cancel_event.is_set():
                break
            try:
                self._emit(f"Eliminando {target}")
                if target.is_dir():
                    shutil.rmtree(target)
                elif target.exists():
                    target.unlink()
                removed += 1
                self._emit(f"Eliminado: {target}", "success")
            except OSError as exc:
                detail = f"{target}: {exc}"
                failures.append(detail)
                self._emit(f"No se pudo eliminar {detail}", "error")
            self.progress.emit(index, total)
        elapsed = time.monotonic() - started
        if self.cancel_event.is_set():
            status = "cancelled"
            self._emit("Limpieza cancelada.", "warn")
        elif failures:
            status = "failed"
            self._emit(f"Limpieza terminada con {len(failures)} error(es).", "error")
        else:
            status = "passed"
            self._emit(f"Limpieza completada: {removed} destino(s) eliminado(s).", "success")
        self.finished.emit(
            {
                "status": status,
                "seconds": elapsed,
                "removed": removed,
                "failures": failures,
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
        self.unit_groups: dict[str, list[str]] = {}
        self.harness_groups: dict[str, list[str]] = {}
        self.language_groups: dict[str, list[str]] = {}
        self.ctest_targets: dict[str, str] = {}
        # Evita la recursion cuando marcar una categoria marca a sus miembros y estos, al cambiar,
        # querrian recalcular la categoria.
        self._selection_guard = False
        self.thread: QThread | None = None
        self.worker: PipelineWorker | CleanupWorker | None = None
        self.inventory_process: QProcess | None = None
        self.inventory_bytes = bytearray()
        self.current_run_dir: Path | None = None
        self.stage_rows_by_name: dict[str, list[tuple[str, int]]] = {}
        self.active_result_channels: list[str] = ["Compilador"]
        self.baseline_cleanup_available = False
        self.close_after_cancel = False
        self._build_ui()
        self._load_settings()
        self._populate_cases()
        QTimer.singleShot(0, self.refresh_ctests)
        QTimer.singleShot(0, self.refresh_history)

    def _build_ui(self) -> None:
        central = QWidget()
        central.setMaximumSize(0, 0)
        self.setCentralWidget(central)
        self.setDockNestingEnabled(True)
        config_panel = QWidget()
        config_layout = QVBoxLayout(config_panel)

        options = QGroupBox("Construcción de nsc")
        grid = QGridLayout(options)
        self.build_mode = QComboBox()
        self.build_mode.addItem("Desde cero", "clean")
        self.build_mode.addItem("Incremental", "incremental")
        self.build_runner = QComboBox()
        self.build_runner.addItem("CMake (cmake --build)", "cmake")
        self.build_runner.addItem("Ninja directo", "ninja")
        self.host_linkage = QComboBox()
        self.host_linkage.addItem("Estático — certificable", "STATIC")
        self.host_linkage.addItem("Dinámico LLVM — desarrollo", "DYNAMIC")
        self.host_optimization = QComboBox()
        for host_optimization in ("O0", "O1", "O2", "O3"):
            self.host_optimization.addItem(host_optimization, host_optimization)
        self.host_optimization.setCurrentIndex(2)
        self.compiler_jobs = QSpinBox()
        self.compiler_jobs.setRange(1, 128)
        self.compiler_jobs.setValue(min(os.cpu_count() or 4, 16))
        self.opt_o0 = QCheckBox("O0")
        self.opt_o0.setChecked(True)
        self.opt_o1 = QCheckBox("O1")
        self.opt_o1.setChecked(True)
        self.opt_o2 = QCheckBox("O2")
        self.opt_o2.setChecked(True)
        self.opt_o3 = QCheckBox("O3")
        self.opt_o3.setChecked(True)
        opt_box = QWidget()
        opt_layout = QHBoxLayout(opt_box)
        opt_layout.setContentsMargins(0, 0, 0, 0)
        opt_layout.addWidget(self.opt_o0)
        opt_layout.addWidget(self.opt_o1)
        opt_layout.addWidget(self.opt_o2)
        opt_layout.addWidget(self.opt_o3)
        opt_layout.addStretch()

        grid.addWidget(QLabel("Construcción"), 0, 0)
        grid.addWidget(self.build_mode, 0, 1)
        grid.addWidget(QLabel("Motor"), 1, 0)
        grid.addWidget(self.build_runner, 1, 1)
        grid.addWidget(QLabel("Enlace LLVM de nsc"), 2, 0)
        grid.addWidget(self.host_linkage, 2, 1)
        grid.addWidget(QLabel("Optimización de nsc"), 3, 0)
        grid.addWidget(self.host_optimization, 3, 1)
        grid.addWidget(QLabel("Procesos en paralelo"), 4, 0)
        grid.addWidget(self.compiler_jobs, 4, 1)
        build_note = QLabel(
            "Sólo reconstruye e instala nsc. El compilador usa siempre clang-cl + lld-link; "
            "NSLink no participa. Sólo el enlace estático puede certificar Gate 0."
        )
        build_note.setWordWrap(True)
        build_note.setObjectName("muted")
        grid.addWidget(build_note, 5, 0, 1, 2)
        self.compiler_summary = QLabel()
        self.compiler_summary.setWordWrap(True)
        self.compiler_summary.setObjectName("planSummary")
        grid.addWidget(self.compiler_summary, 6, 0, 1, 2)
        self.compiler_start_button = QPushButton("Construir e instalar nsc")
        self.compiler_start_button.clicked.connect(self._start_compiler)
        self.compiler_install_button = QPushButton("Instalar build existente")
        self.compiler_install_button.setToolTip(
            "Instala el contenido ya construido sin configurar ni recompilar"
        )
        self.compiler_install_button.clicked.connect(self._start_install)
        compiler_buttons = QWidget()
        compiler_buttons_layout = QHBoxLayout(compiler_buttons)
        compiler_buttons_layout.setContentsMargins(0, 0, 0, 0)
        compiler_buttons_layout.addWidget(self.compiler_start_button)
        compiler_buttons_layout.addWidget(self.compiler_install_button)
        grid.addWidget(compiler_buttons, 7, 0, 1, 2)
        config_layout.addWidget(options)

        # Estos controles pertenecen exclusivamente al panel de pruebas.
        self.stage_ctest = QCheckBox("CTest")
        self.stage_ctest.setChecked(True)
        self.stage_cases = QCheckBox("Fixtures y ejemplos")
        self.stage_cases.setChecked(True)
        self.stage_triage = QCheckBox("Bug triage")
        self.stage_triage.setChecked(True)
        self.keep_temps = QCheckBox("Conservar temporales de enlace")

        paths = QGroupBox("Rutas del compilador")
        form = QFormLayout(paths)
        self.build_path = QLineEdit(str(DEFAULT_BUILD))
        self.prefix_path = QLineEdit(str(DEFAULT_PREFIX))
        self.llvm_path = QLineEdit(str(DEFAULT_LLVM))
        form.addRow("Build de nsc", self.build_path)
        form.addRow("Instalar en", self.prefix_path)
        form.addRow("LLVM", self.llvm_path)
        config_layout.addWidget(paths)
        config_layout.addStretch()

        selection = QWidget()
        self.test_panel = selection
        selection_layout = QVBoxLayout(selection)
        test_options = QGroupBox("Compilación y ejecución de pruebas")
        test_grid = QGridLayout(test_options)
        self.test_build_lld_path = QLineEdit(str(DEFAULT_TEST_BUILD_LLD))
        self.test_build_nslink_path = QLineEdit(str(DEFAULT_TEST_BUILD_NSLINK))
        self.test_prefix_path = QLineEdit(str(DEFAULT_PREFIX))
        self.test_llvm_path = QLineEdit(str(DEFAULT_LLVM))
        self.nslink_path = QLineEdit(str(DEFAULT_NSLINK))
        self.test_build_runner = QComboBox()
        self.test_build_runner.addItem("CMake", "cmake")
        self.test_build_runner.addItem("Ninja", "ninja")
        self.linker_mode = QComboBox()
        self.linker_mode.addItem("LLD-link", ["lld-link"])
        self.linker_mode.addItem("NSLink", ["NSLink"])
        self.linker_mode.addItem("Ambos (corridas independientes)", ["lld-link", "NSLink"])
        self.test_jobs = QSpinBox()
        self.test_jobs.setRange(1, 128)
        self.test_jobs.setValue(min(os.cpu_count() or 4, 16))
        test_grid.addWidget(QLabel("Motor de construcción"), 0, 0)
        test_grid.addWidget(self.test_build_runner, 0, 1)
        test_grid.addWidget(QLabel("Build CTest — LLD-link"), 1, 0)
        test_grid.addWidget(self.test_build_lld_path, 1, 1, 1, 3)
        test_grid.addWidget(QLabel("Build CTest — NSLink"), 2, 0)
        test_grid.addWidget(self.test_build_nslink_path, 2, 1, 1, 3)
        test_grid.addWidget(QLabel("Optimización"), 3, 2)
        test_grid.addWidget(opt_box, 3, 3)
        test_grid.addWidget(QLabel("Procesos en paralelo"), 4, 0)
        test_grid.addWidget(self.test_jobs, 4, 1)
        test_grid.addWidget(QLabel("Instalación de nsc"), 5, 0)
        test_grid.addWidget(self.test_prefix_path, 5, 1, 1, 3)
        test_grid.addWidget(QLabel("LLVM / LLD-link"), 6, 0)
        test_grid.addWidget(self.test_llvm_path, 6, 1, 1, 3)
        test_grid.addWidget(QLabel("Ejecutable NSLink"), 7, 0)
        test_grid.addWidget(self.nslink_path, 7, 1, 1, 3)
        test_grid.addWidget(self.keep_temps, 8, 0, 1, 4)
        linker_note = QLabel(
            "Esta sección NO COMPILA NADA: ni nsc ni ningún ejecutable de prueba. Usa el nsc "
            "ya instalado para "
            "compilar los programas elegidos. LLD-link y NSLink tienen árboles CTest separados; "
            "cada enlazador y cada nivel O0/O1/O2/O3 conserva además su propia consola, lista de errores "
            "e informe."
        )
        linker_note.setWordWrap(True)
        linker_note.setObjectName("muted")
        test_grid.addWidget(linker_note, 9, 0, 1, 4)
        test_grid.setColumnStretch(1, 1)
        test_grid.setColumnStretch(3, 2)
        selection_layout.addWidget(test_options)

        test_sets = QGroupBox("Conjuntos que se ejecutarán")
        test_sets_layout = QHBoxLayout(test_sets)
        for widget in (self.stage_ctest, self.stage_cases, self.stage_triage):
            test_sets_layout.addWidget(widget)
        test_sets_layout.addStretch()
        selection_layout.addWidget(test_sets)
        selection_tabs = QTabWidget()
        selection_tabs.addTab(self._language_ctest_page(), "CTest de lenguaje")
        selection_tabs.addTab(self._case_page(), "Programas nsc")
        selection_layout.addWidget(selection_tabs, 1)
        self.test_summary = QLabel()
        self.test_summary.setWordWrap(True)
        self.test_summary.setObjectName("planSummary")
        selection_layout.addWidget(self.test_summary)
        self.tests_start_button = QPushButton("Ejecutar pruebas de lenguaje")
        self.tests_start_button.clicked.connect(self._start_tests)
        selection_layout.addWidget(self.tests_start_button)

        # LA PESTANA DE UNITARIAS, autocontenida: su seleccion, su construccion y su ejecucion.
        unit_panel = QWidget()
        unit_layout = QVBoxLayout(unit_panel)
        unit_note = QLabel(
            "Ejecutables de C++ que miden el compilador POR DENTRO. Construye únicamente los "
            "ejecutables de las pruebas marcadas; nunca reconstruye nsc."
        )
        unit_note.setWordWrap(True)
        unit_note.setObjectName("muted")
        unit_layout.addWidget(unit_note)
        unit_layout.addWidget(self._unit_page(), 1)
        unit_start = QPushButton("Construir y ejecutar pruebas unitarias")
        unit_start.clicked.connect(self._start_unit)
        unit_layout.addWidget(unit_start)

        # LA PESTANA DEL ARNES. Aparte porque tiene una dependencia que las demas no tienen.
        harness_panel = QWidget()
        harness_layout = QVBoxLayout(harness_panel)
        harness_note = QLabel(
            "Miden el LENGUAJE --compilan fixtures `.ns` con el nsc instalado-- pero comparten un "
            "arnés de C++ que sí hay que construir. Están aparte para que ni las unitarias ni las "
            "de lenguaje carguen con esa dependencia."
        )
        harness_note.setWordWrap(True)
        harness_note.setObjectName("muted")
        harness_layout.addWidget(harness_note)
        harness_layout.addWidget(self._harness_page(), 1)
        harness_start = QPushButton("Construir el arnés y ejecutar")
        harness_start.clicked.connect(self._start_harness)
        harness_layout.addWidget(harness_start)
        # EL ENLAZADOR ES COMUN A LAS TRES PESTANAS DE PRUEBA, asi que vive FUERA de ellas.
        #
        # Estaba dentro del grupo de la pestana de lenguaje, donde no se ve desde «unitarias» ni
        # desde «con arnés» -- y las tres lo necesitan, porque decide contra que arbol de CTest se
        # trabaja (`build-test-lldlinker` o `build-test-nslink`) y, en las de lenguaje, que
        # enlazador usa nsc al construir los programas. Un ajuste que gobierna tres pestanas no
        # puede vivir dentro de una.
        linker_bar = QWidget()
        linker_bar_layout = QHBoxLayout(linker_bar)
        linker_bar_layout.setContentsMargins(0, 0, 0, 0)
        linker_bar_layout.addWidget(QLabel("Enlazador usado por nsc"))
        linker_bar_layout.addWidget(self.linker_mode, 1)
        self.linker_bar_note = QLabel()
        self.linker_bar_note.setObjectName("muted")
        self.linker_bar_note.setWordWrap(True)
        linker_bar_layout.addWidget(self.linker_bar_note, 2)

        operations_panel = QWidget()
        operations_layout = QVBoxLayout(operations_panel)
        operations_layout.setContentsMargins(0, 0, 0, 0)
        operations_layout.addWidget(linker_bar)

        self.operation_tabs = QTabWidget()
        self.operation_tabs.addTab(config_panel, "Compilador")
        self.operation_tabs.addTab(unit_panel, "Pruebas unitarias")
        self.operation_tabs.addTab(harness_panel, "Pruebas con arnés")
        self.operation_tabs.addTab(selection, "Pruebas de lenguaje")
        self.operation_tabs.addTab(self._abi_page(), "ABI")
        operations_layout.addWidget(self.operation_tabs, 1)
        self.operation_tabs.currentChanged.connect(self._update_linker_note)
        self.operations_dock = self._create_dock(
            "Operaciones",
            operations_panel,
            Qt.DockWidgetArea.LeftDockWidgetArea,
            "dock_operations",
        )

        console_panel = QWidget()
        right_layout = QVBoxLayout(console_panel)
        controls = QHBoxLayout()
        self.cancel_button = QPushButton("Cancelar")
        self.cancel_button.setEnabled(False)
        self.cancel_button.clicked.connect(self.cancel_run)
        self.open_button = QPushButton("Abrir resultados")
        self.open_button.setEnabled(False)
        self.open_button.clicked.connect(self.open_results)
        self.clear_button = QPushButton("Limpiar consola")
        self.clear_button.clicked.connect(self.clear_current_console)
        self.cleanup_button = QPushButton("Limpiar")
        self.cleanup_button.setToolTip(
            "Borra árboles de compilación y temporales; conserva fuentes, historial y baselines"
        )
        self.cleanup_button.clicked.connect(self.start_cleanup)
        self.status_label = QLabel("Listo")
        controls.addWidget(self.cancel_button)
        controls.addWidget(self.open_button)
        controls.addWidget(self.cleanup_button)
        controls.addWidget(self.clear_button)
        controls.addWidget(self.status_label, 1)
        right_layout.addLayout(controls)

        self.progress_bar = QProgressBar()
        self.progress_bar.setRange(0, 1)
        self.progress_bar.setValue(0)
        right_layout.addWidget(self.progress_bar)

        self.output_channels = ("Compilador", "LLD-link", "NSLink")
        self.output_optimizations = ("General", "O0", "O1", "O2", "O3")
        self.channel_consoles: dict[tuple[str, str], QTextEdit] = {}
        self.channel_error_tables: dict[str, QTableWidget] = {}
        self.channel_error_counts: dict[str, int] = {}
        self.channel_output_tabs: dict[str, QTabWidget] = {}
        self.channel_error_tab_indices: dict[str, int] = {}
        self.channel_results_tab_indices: dict[str, int] = {}
        self.channel_outer_indices: dict[str, int] = {}
        self.channel_titles: dict[str, str] = {}
        self.channel_optimization_tabs: dict[str, QTabWidget] = {}
        self.channel_optimization_indices: dict[tuple[str, str], int] = {}
        self.stage_tables: dict[str, QTableWidget] = {}
        self.output_channel_tabs = QTabWidget()
        for channel in self.output_channels:
            channel_tabs = QTabWidget()
            channel_optimizations = (
                ("General",) if channel == "Compilador" else self.output_optimizations
            )
            for optimization in channel_optimizations:
                key = (channel, optimization)
                console = self._create_console()
                label = "Consola general" if optimization == "General" else f"Consola {optimization}"
                optimization_index = channel_tabs.addTab(console, label)
                self.channel_consoles[key] = console
                self.channel_optimization_indices[key] = optimization_index
            error_table = self._create_error_table()
            error_index = channel_tabs.addTab(error_table, "Errores (0)")
            stage_table = self._create_stage_table()
            results_index = channel_tabs.addTab(stage_table, "Resultados")
            title = channel
            outer_index = self.output_channel_tabs.addTab(channel_tabs, title)
            self.channel_titles[channel] = title
            self.channel_error_tables[channel] = error_table
            self.channel_error_counts[channel] = 0
            self.channel_output_tabs[channel] = channel_tabs
            self.channel_error_tab_indices[channel] = error_index
            self.channel_results_tab_indices[channel] = results_index
            self.channel_optimization_tabs[channel] = channel_tabs
            self.channel_outer_indices[channel] = outer_index
            self.stage_tables[channel] = stage_table
        self.history_outer_index = self.output_channel_tabs.addTab(
            self._create_history_page(), "Historial"
        )
        right_layout.addWidget(self.output_channel_tabs, 3)
        self.console_dock = self._create_dock(
            "Salida y resultados",
            console_panel,
            Qt.DockWidgetArea.RightDockWidgetArea,
            "dock_console",
        )

        self.view_menu = self.menuBar().addMenu("Ver")
        for dock in (
            self.operations_dock,
            self.console_dock,
        ):
            self.view_menu.addAction(dock.toggleViewAction())

        for widget in (
            self.build_mode,
            self.build_runner,
            self.host_linkage,
            self.host_optimization,
            self.test_build_runner,
            self.linker_mode,
        ):
            widget.currentIndexChanged.connect(self._update_plan_summary)
        for widget in (self.stage_ctest, self.stage_cases, self.stage_triage, self.opt_o0, self.opt_o1,
                       self.opt_o2, self.opt_o3):
            widget.toggled.connect(self._update_plan_summary)
        self.compiler_jobs.valueChanged.connect(self._update_plan_summary)
        self.test_jobs.valueChanged.connect(self._update_plan_summary)
        self.test_build_lld_path.textChanged.connect(self._update_plan_summary)
        self.test_build_nslink_path.textChanged.connect(self._update_plan_summary)
        self.test_prefix_path.textChanged.connect(self._update_plan_summary)
        self.test_llvm_path.textChanged.connect(self._update_plan_summary)
        self._update_plan_summary()
        self._update_linker_note()

    def _create_console(self) -> QTextEdit:
        console = QTextEdit()
        console.setReadOnly(True)
        console.setAcceptRichText(False)
        console.setLineWrapMode(QTextEdit.LineWrapMode.NoWrap)
        console.setFont(QFontDatabase.systemFont(QFontDatabase.SystemFont.FixedFont))
        return console

    def _create_error_table(self) -> QTableWidget:
        table = QTableWidget(0, 5)
        table.setHorizontalHeaderLabels(
            ["Optimización", "Conjunto", "Prueba o etapa", "Error", "Log"]
        )
        table.verticalHeader().setVisible(False)
        table.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        table.setSelectionMode(QAbstractItemView.SelectionMode.SingleSelection)
        table.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers)
        table.horizontalHeader().setSectionResizeMode(0, QHeaderView.ResizeMode.ResizeToContents)
        table.horizontalHeader().setSectionResizeMode(1, QHeaderView.ResizeMode.ResizeToContents)
        table.horizontalHeader().setSectionResizeMode(2, QHeaderView.ResizeMode.ResizeToContents)
        table.horizontalHeader().setSectionResizeMode(3, QHeaderView.ResizeMode.Stretch)
        table.horizontalHeader().setSectionResizeMode(4, QHeaderView.ResizeMode.ResizeToContents)
        table.itemDoubleClicked.connect(self.open_error_log)
        return table

    def _create_stage_table(self) -> QTableWidget:
        table = QTableWidget(0, 4)
        table.setHorizontalHeaderLabels(["Etapa", "Estado", "Tiempo", "Exit"])
        table.verticalHeader().setVisible(False)
        table.horizontalHeader().setSectionResizeMode(0, QHeaderView.ResizeMode.Stretch)
        table.horizontalHeader().setSectionResizeMode(1, QHeaderView.ResizeMode.ResizeToContents)
        table.horizontalHeader().setSectionResizeMode(2, QHeaderView.ResizeMode.ResizeToContents)
        table.horizontalHeader().setSectionResizeMode(3, QHeaderView.ResizeMode.ResizeToContents)
        table.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        table.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers)
        return table

    def _create_history_page(self) -> QWidget:
        page = QWidget()
        layout = QVBoxLayout(page)
        controls = QHBoxLayout()
        explanation = QLabel(
            "Cada fila es una corrida conservada. Preparación incluye configurar, construir, "
            "instalar y sincronizar; Pruebas incluye CTest, fixtures y bug triage. "
            "Doble clic abre el reporte."
        )
        explanation.setWordWrap(True)
        explanation.setObjectName("muted")
        controls.addWidget(explanation, 1)
        refresh = QPushButton("Actualizar historial")
        refresh.clicked.connect(self.refresh_history)
        controls.addWidget(refresh)
        layout.addLayout(controls)

        self.history_table = QTableWidget(0, 11)
        self.history_table.setHorizontalHeaderLabels(
            [
                "Corrida", "Inicio", "Estado", "Operación", "Total", "Preparación",
                "Pruebas", "Fallos", "Modo", "Enlazadores", "Reporte",
            ]
        )
        self.history_table.verticalHeader().setVisible(False)
        self.history_table.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        self.history_table.setSelectionMode(QAbstractItemView.SelectionMode.SingleSelection)
        self.history_table.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers)
        self.history_table.horizontalHeader().setSectionResizeMode(
            0, QHeaderView.ResizeMode.Stretch
        )
        for column in range(1, 11):
            self.history_table.horizontalHeader().setSectionResizeMode(
                column, QHeaderView.ResizeMode.ResizeToContents
            )
        self.history_table.itemDoubleClicked.connect(self.open_history_report)
        layout.addWidget(self.history_table, 1)
        return page

    @staticmethod
    def _history_duration(value: object) -> str:
        try:
            seconds = max(0.0, float(value))
        except (TypeError, ValueError):
            seconds = 0.0
        hours, remainder = divmod(int(round(seconds)), 3600)
        minutes, whole_seconds = divmod(remainder, 60)
        if hours:
            return f"{hours:d} h {minutes:02d} min {whole_seconds:02d} s"
        if minutes:
            return f"{minutes:d} min {whole_seconds:02d} s"
        return f"{seconds:.2f} s"

    @Slot()
    def refresh_history(self) -> None:
        entries = load_run_history(HISTORY_ROOT)
        self.history_table.setRowCount(0)
        status_names = {
            "passed": "correcta",
            "failed": "fallida",
            "cancelled": "cancelada",
            "running": "en curso",
        }
        operation_names = {
            "baseline": "baseline",
            "compiler": "compilador",
            "install": "instalación",
            "tests": "pruebas",
            "unit": "unitarias",
            "harness": "arnés",
        }
        for entry in entries:
            row = self.history_table.rowCount()
            self.history_table.insertRow(row)
            build_mode = str(entry.get("build_mode", ""))
            test_mode = str(entry.get("test_build_mode", ""))
            modes = [value for value in (build_mode, test_mode) if value and value != "skip"]
            mode = " / ".join(dict.fromkeys(modes)) or "—"
            linkers_value = entry.get("linkers", [])
            linkers = ", ".join(str(value) for value in linkers_value) \
                if isinstance(linkers_value, list) else ""
            status = str(entry.get("status", "unknown"))
            operation = str(entry.get("operation", ""))
            values = (
                str(entry.get("id", "")),
                str(entry.get("started_at", "")),
                status_names.get(status, status),
                operation_names.get(operation, operation or "—"),
                self._history_duration(entry.get("total_seconds", 0.0)),
                self._history_duration(entry.get("build_seconds", 0.0)),
                self._history_duration(entry.get("test_seconds", 0.0)),
                str(entry.get("failed", 0)),
                mode,
                linkers or "—",
                "report.md",
            )
            report = str(entry.get("report", ""))
            run_dir = str(entry.get("run_dir", ""))
            for column, value in enumerate(values):
                item = QTableWidgetItem(value)
                item.setData(Qt.ItemDataRole.UserRole, report)
                item.setData(Qt.ItemDataRole.UserRole + 1, run_dir)
                item.setToolTip(f"Doble clic para abrir {report or run_dir}")
                self.history_table.setItem(row, column, item)
        self.output_channel_tabs.setTabText(
            self.history_outer_index, f"Historial ({len(entries)})"
        )

    @Slot(QTableWidgetItem)
    def open_history_report(self, item: QTableWidgetItem) -> None:
        report = Path(str(item.data(Qt.ItemDataRole.UserRole) or ""))
        run_dir = Path(str(item.data(Qt.ItemDataRole.UserRole + 1) or ""))
        destination = report if report.is_file() else run_dir
        if destination.exists():
            QDesktopServices.openUrl(QUrl.fromLocalFile(str(destination)))
            return
        QMessageBox.warning(self, APP_NAME, f"La evidencia ya no existe:\n{destination}")

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

    # SELECCION POR CASILLAS, Y POR QUE SE CAMBIO LO QUE HABIA.
    #
    # Antes eran tres combos --«todas / un grupo / una sola»-- y una lista de solo lectura. Con eso
    # no se podian correr DOS pruebas concretas, ni dos categorias, que es justo lo que hace falta
    # a diario: se toca un fichero y se quieren las tres o cuatro suites que dependen de el. La
    # lista, ademas, no decia que se iba a ejecutar: mostraba todo y resaltaba una.
    #
    # Ahora hay casillas en las pruebas y casillas en las categorias, y la categoria refleja a sus
    # miembros --marcada, vacia o A MEDIAS--. El estado a medias no es cosmetico: sin el, una
    # categoria con tres de treinta marcadas se veria igual que una entera.
    #
    # NADA MARCADO SIGNIFICA TODO, que es el contrato que ya tenia el plan (`selected == []`). El
    # resumen lo dice con todas las letras en vez de dejarlo a la adivinanza.
    def _selection_page(
        self,
        group_title: str,
        item_title: str,
        filter_hint: str,
        run_text: str,
        run_slot,
        groups,
        refresh_slot=None,
    ) -> tuple[QWidget, QListWidget, QListWidget, QLabel]:
        page = QWidget()
        layout = QVBoxLayout(page)

        row = QHBoxLayout()
        text_filter = QLineEdit()
        text_filter.setPlaceholderText(filter_hint)
        text_filter.setClearButtonEnabled(True)
        visible_label = QLabel()
        row.addWidget(text_filter, 1)
        row.addWidget(visible_label)
        if refresh_slot is not None:
            refresh = QPushButton("Actualizar")
            refresh.clicked.connect(refresh_slot)
            row.addWidget(refresh)
        run = QPushButton(run_text)
        run.clicked.connect(run_slot)
        row.addWidget(run)
        layout.addLayout(row)

        lists = QHBoxLayout()
        group_box = QGroupBox(group_title)
        group_layout = QVBoxLayout(group_box)
        group_list = QListWidget()
        group_list.setSelectionMode(QAbstractItemView.SelectionMode.NoSelection)
        group_layout.addWidget(group_list)
        lists.addWidget(group_box, 2)

        item_box = QGroupBox(item_title)
        item_layout = QVBoxLayout(item_box)
        item_list = QListWidget()
        item_list.setSelectionMode(QAbstractItemView.SelectionMode.NoSelection)
        item_layout.addWidget(item_list)
        buttons = QHBoxLayout()
        for label, slot in (
            ("Marcar todo", lambda: self._set_all_checks(item_list, True, visible_only=False)),
            ("Desmarcar todo", lambda: self._set_all_checks(item_list, False, visible_only=False)),
            # Con un filtro escrito, «marcar lo visible» es la operacion que de verdad se quiere:
            # se busca «stage56», se marca lo que salio, y se corre eso.
            ("Marcar lo visible", lambda: self._set_all_checks(item_list, True, visible_only=True)),
            ("Invertir", lambda: self._invert_checks(item_list)),
        ):
            button = QPushButton(label)
            button.clicked.connect(slot)
            buttons.addWidget(button)
        item_layout.addLayout(buttons)
        lists.addWidget(item_box, 5)
        layout.addLayout(lists)

        summary = QLabel()
        summary.setWordWrap(True)
        layout.addWidget(summary)

        text_filter.textChanged.connect(
            lambda text: self._apply_filter(item_list, group_list, groups(), visible_label, text)
        )
        # ENTER MARCA LO VISIBLE: escribir, mirar, ejecutar, sin soltar el teclado. SOLO con algo
        # escrito -- con el filtro vacio «lo visible» son todas, y marcarlas de un Enter suelto
        # seria justo la sorpresa que este panel existe para evitar.
        text_filter.returnPressed.connect(
            lambda: self._set_all_checks(item_list, True, visible_only=True)
            if text_filter.text().strip()
            else None
        )
        focus = QShortcut(QKeySequence.StandardKey.Find, page)
        focus.activated.connect(text_filter.setFocus)
        clear = QShortcut(QKeySequence(Qt.Key.Key_Escape), text_filter)
        clear.activated.connect(text_filter.clear)
        return page, group_list, item_list, summary

    def _unit_page(self) -> QWidget:
        """Pruebas UNITARIAS: ejecutables de C++ que miden el compilador por dentro."""
        page, groups, items, summary = self._selection_page(
            "Categorías",
            "Pruebas unitarias",
            "Filtrar nombres...",
            "Construir y ejecutar la selección",
            self._run_selected_unit,
            lambda: self.unit_groups,
            self.refresh_ctests,
        )
        self.unit_group_list = groups
        self.unit_list = items
        self.unit_scope_label = summary
        groups.itemChanged.connect(self._unit_group_toggled)
        items.itemChanged.connect(self._selection_item_toggled)
        return page

    def _harness_page(self) -> QWidget:
        """Pruebas del LENGUAJE que cuelgan de un arnés de C++ compartido.

        Están aparte porque tienen una dependencia que las demás pruebas de lenguaje no tienen.
        Meterlas con ellas obligaría a construir C++ para correr un programa `.ns`; meterlas con
        las unitarias diría que miden el compilador por dentro, que es falso.
        """
        page, groups, items, summary = self._selection_page(
            "Categorías",
            "Pruebas con arnés",
            "Filtrar nombres...",
            "Construir el arnés y ejecutar la selección",
            self._run_selected_harness,
            lambda: self.harness_groups,
            self.refresh_ctests,
        )
        self.harness_group_list = groups
        self.harness_list = items
        self.harness_scope_label = summary
        groups.itemChanged.connect(self._harness_group_toggled)
        items.itemChanged.connect(self._selection_item_toggled)
        return page

    def _language_ctest_page(self) -> QWidget:
        """Pruebas de LENGUAJE que viven en CTest: invocan el `nsc.exe` instalado."""
        page, groups, items, summary = self._selection_page(
            "Categorías",
            "Pruebas de lenguaje en CTest",
            "Filtrar nombres...",
            "Ejecutar la selección",
            self._run_selected_language,
            lambda: self.language_groups,
            self.refresh_ctests,
        )
        self.language_group_list = groups
        self.language_list = items
        self.language_scope_label = summary
        groups.itemChanged.connect(self._language_group_toggled)
        items.itemChanged.connect(self._selection_item_toggled)
        return page

    def _case_page(self) -> QWidget:
        page, groups, items, summary = self._selection_page(
            "Categorías",
            "Fixtures y ejemplos",
            "Filtrar casos...",
            "Ejecutar la selección",
            self._run_selected_cases,
            lambda: self.case_groups,
        )
        self.case_group_list = groups
        self.case_list = items
        self.case_scope_label = summary
        groups.itemChanged.connect(self._case_group_toggled)
        items.itemChanged.connect(self._selection_item_toggled)
        return page

    # ------------------------------------------------------------------ casillas

    @staticmethod
    def _checkable(text: str, value: str) -> QListWidgetItem:
        item = QListWidgetItem(text)
        item.setFlags(item.flags() | Qt.ItemFlag.ItemIsUserCheckable)
        item.setCheckState(Qt.CheckState.Unchecked)
        item.setData(Qt.ItemDataRole.UserRole, value)
        return item

    @staticmethod
    def _checked_values(widget: QListWidget) -> list[str]:
        values: list[str] = []
        for index in range(widget.count()):
            item = widget.item(index)
            if item.checkState() == Qt.CheckState.Checked:
                values.append(str(item.data(Qt.ItemDataRole.UserRole) or item.text()))
        return values

    def _set_all_checks(self, widget: QListWidget, checked: bool, *, visible_only: bool) -> None:
        state = Qt.CheckState.Checked if checked else Qt.CheckState.Unchecked
        # Una sola pasada silenciosa y UN refresco al final: emitir itemChanged 291 veces
        # recalcularia las categorias 291 veces y la ventana se quedaria pensando.
        self._selection_guard = True
        try:
            for index in range(widget.count()):
                item = widget.item(index)
                if visible_only and item.isHidden():
                    continue
                item.setCheckState(state)
        finally:
            self._selection_guard = False
        self._refresh_selection()

    def _invert_checks(self, widget: QListWidget) -> None:
        self._selection_guard = True
        try:
            for index in range(widget.count()):
                item = widget.item(index)
                item.setCheckState(
                    Qt.CheckState.Unchecked
                    if item.checkState() == Qt.CheckState.Checked
                    else Qt.CheckState.Checked
                )
        finally:
            self._selection_guard = False
        self._refresh_selection()

    def _apply_group_check(
        self, group_item: QListWidgetItem, item_list: QListWidget, members: list[str]
    ) -> None:
        """Marcar o desmarcar una categoría arrastra a sus miembros."""
        state = group_item.checkState()
        if state == Qt.CheckState.PartiallyChecked:
            return
        wanted = set(members)
        self._selection_guard = True
        try:
            for index in range(item_list.count()):
                item = item_list.item(index)
                value = str(item.data(Qt.ItemDataRole.UserRole) or item.text())
                if value in wanted:
                    item.setCheckState(state)
        finally:
            self._selection_guard = False

    def _sync_group_states(
        self, group_list: QListWidget, item_list: QListWidget, groups: dict[str, list[str]]
    ) -> None:
        """El estado de cada categoría SALE de sus miembros, nunca al revés."""
        checked = set(self._checked_values(item_list))
        self._selection_guard = True
        try:
            for index in range(group_list.count()):
                group_item = group_list.item(index)
                name = str(group_item.data(Qt.ItemDataRole.UserRole) or "")
                members = groups.get(name, [])
                marked = sum(1 for member in members if member in checked)
                if not members or marked == 0:
                    state = Qt.CheckState.Unchecked
                elif marked == len(members):
                    state = Qt.CheckState.Checked
                else:
                    state = Qt.CheckState.PartiallyChecked
                group_item.setCheckState(state)
                group_item.setText(f"{name} ({marked}/{len(members)})")
        finally:
            self._selection_guard = False

    @Slot(QListWidgetItem)
    def _unit_group_toggled(self, item: QListWidgetItem) -> None:
        if self._selection_guard:
            return
        name = str(item.data(Qt.ItemDataRole.UserRole) or "")
        self._apply_group_check(item, self.unit_list, self.unit_groups.get(name, []))
        self._refresh_selection()

    @Slot(QListWidgetItem)
    def _harness_group_toggled(self, item: QListWidgetItem) -> None:
        if self._selection_guard:
            return
        name = str(item.data(Qt.ItemDataRole.UserRole) or "")
        self._apply_group_check(item, self.harness_list, self.harness_groups.get(name, []))
        self._refresh_selection()

    @Slot(QListWidgetItem)
    def _language_group_toggled(self, item: QListWidgetItem) -> None:
        if self._selection_guard:
            return
        name = str(item.data(Qt.ItemDataRole.UserRole) or "")
        self._apply_group_check(item, self.language_list, self.language_groups.get(name, []))
        self._refresh_selection()

    @Slot(QListWidgetItem)
    def _case_group_toggled(self, item: QListWidgetItem) -> None:
        if self._selection_guard:
            return
        name = str(item.data(Qt.ItemDataRole.UserRole) or "")
        self._apply_group_check(item, self.case_list, self.case_groups.get(name, []))
        self._refresh_selection()

    @Slot(QListWidgetItem)
    def _selection_item_toggled(self, _item: QListWidgetItem) -> None:
        if self._selection_guard:
            return
        self._refresh_selection()

    def _refresh_selection(self) -> None:
        if not hasattr(self, "unit_list") or not hasattr(self, "case_list"):
            return
        self._sync_group_states(self.unit_group_list, self.unit_list, self.unit_groups)
        self._sync_group_states(self.harness_group_list, self.harness_list, self.harness_groups)
        self._sync_group_states(
            self.language_group_list, self.language_list, self.language_groups)
        self._sync_group_states(self.case_group_list, self.case_list, self.case_groups)
        self._update_unit_scope()
        self._update_harness_scope()
        self._update_language_scope()
        self._update_case_scope()

    def _apply_filter(
        self,
        item_list: QListWidget,
        group_list: QListWidget,
        groups: dict[str, list[str]],
        visible_label: QLabel,
        text: str,
    ) -> None:
        """Filtrar OCULTA, nunca desmarca: lo escondido sigue contando y el resumen lo dice."""
        query = text.strip()
        visible: set[str] = set()
        for index in range(item_list.count()):
            item = item_list.item(index)
            value = str(item.data(Qt.ItemDataRole.UserRole) or item.text())
            shown = not query or matches_filter(item.text(), query)
            item.setHidden(not shown)
            if shown:
                visible.add(value)
        # Las categorias se acotan igual: dejar las quince mientras la lista muestra cuatro
        # pruebas convierte esa columna en ruido, y marcar una que ya no tiene nada visible
        # marcaria cosas que el usuario no esta viendo.
        for index in range(group_list.count()):
            group_item = group_list.item(index)
            name = str(group_item.data(Qt.ItemDataRole.UserRole) or "")
            members = groups.get(name, [])
            group_item.setHidden(bool(query) and not any(m in visible for m in members))
        visible_label.setText("" if not query else f"{len(visible)} de {item_list.count()}")
        self._refresh_selection()

    def _populate_cases(self) -> None:
        self._selection_guard = True
        try:
            self.case_list.clear()
            self.case_group_list.clear()
            self.case_groups.clear()
            for case in sorted(self.cases, key=lambda item: item.label.casefold()):
                item = self._checkable(case.label, case.label)
                item.setToolTip(
                    f"{case.project}\nEsperado: {case.expected}\nFuente: {case.source}")
                self.case_list.addItem(item)
                self.case_groups.setdefault(case_group(case), []).append(case.label)
            for group in sorted(self.case_groups, key=str.casefold):
                members = sorted(self.case_groups[group], key=str.casefold)
                self.case_groups[group] = members
                self.case_group_list.addItem(
                    self._checkable(f"{group} (0/{len(members)})", group))
        finally:
            self._selection_guard = False
        self._refresh_selection()

    def _selected_unit_values(self) -> list[str]:
        """Las marcadas. Vacío significa TODAS, que es el contrato del plan."""
        return self._checked_values(self.unit_list)

    def _selected_harness_values(self) -> list[str]:
        return self._checked_values(self.harness_list)

    def _selected_language_values(self) -> list[str]:
        return self._checked_values(self.language_list)

    def _selected_case_values(self) -> list[str]:
        return self._checked_values(self.case_list)

    @staticmethod
    def _hidden_checked(widget: QListWidget) -> int:
        """Marcadas pero escondidas por el filtro. Callarlas seria mentir sobre lo que va a correr."""
        return sum(
            1
            for index in range(widget.count())
            if widget.item(index).isHidden()
            and widget.item(index).checkState() == Qt.CheckState.Checked
        )

    @Slot()
    def _update_linker_note(self, *_args: object) -> None:
        """Qué significa el enlazador en la pestaña que se está mirando. No es lo mismo en cada una.

        MEDIDO, y por eso el aviso: `_configure_command` genera EXACTAMENTE la misma línea para
        `build-test-lldlinker` y `build-test-nslink` -- solo cambia el `-B`. Lo único que separa a
        los dos árboles es la variable `NSC_LLD_LINK` en tiempo de EJECUCIÓN. De ahí que en las
        pestañas que solo construyen y ejecutan binarios de C++, elegir «Ambos» haga dos veces
        exactamente el mismo trabajo.
        """
        if not hasattr(self, "linker_bar_note"):
            return
        titles = {
            "Compilador": "No aplica: nsc siempre se construye con clang y lld-link.",
            "Pruebas unitarias": (
                "Elige el árbol de CTest. Los dos producen los MISMOS ejecutables, así que "
                "«Ambos» construye y ejecuta dos veces lo mismo."
            ),
            "Pruebas con arnés": (
                "Elige el árbol de CTest. El arnés es idéntico en los dos; lo que cambia es el "
                "enlazador con el que nsc construye los fixtures que el arnés compila."
            ),
            "Pruebas de lenguaje": (
                "El enlazador que usa nsc al construir los programas de prueba. Aquí sí cambia lo "
                "que se mide."
            ),
            "ABI": "El Gate 0 usa siempre los dos enlazadores; esta elección no lo afecta.",
        }
        index = self.operation_tabs.currentIndex()
        title = self.operation_tabs.tabText(index) if index >= 0 else ""
        self.linker_bar_note.setText(titles.get(title, ""))

    @Slot()
    def _update_unit_scope(self, *_args: object) -> None:
        total = self.unit_list.count()
        selected = self._selected_unit_values()
        if not selected:
            detail = (
                f"Sin marcar nada: se construirán y ejecutarán las {total} pruebas unitarias."
                if total
                else "No hay inventario de CTest todavía. Pulsa «Actualizar»."
            )
        else:
            target = self._targets_for_ctests(selected)
            names = [name for name in target.split(";") if name]
            if target == "pruebas":
                cost = "construirá TODOS los ejecutables (alguna no se pudo resolver)"
            else:
                cost = f"construirá {len(names)} ejecutable(s)"
            detail = f"Se ejecutarán {len(selected)} de {total} pruebas unitarias · {cost}."
            hidden = self._hidden_checked(self.unit_list)
            if hidden:
                detail += f" ({hidden} marcada(s) están ocultas por el filtro.)"
        self.unit_scope_label.setText(detail)
        self._update_plan_summary()

    @Slot()
    def _update_harness_scope(self, *_args: object) -> None:
        total = self.harness_list.count()
        selected = self._selected_harness_values()
        if not selected:
            detail = f"Sin marcar nada: se ejecutarán las {total} pruebas con arnés."
        else:
            target = self._targets_for_ctests(selected)
            names = [name for name in target.split(";") if name]
            cost = (
                "construirá TODOS los ejecutables (alguna no se pudo resolver)"
                if target == "pruebas"
                else f"construirá {len(names)} arnés/arneses: " + ", ".join(sorted(names))
            )
            detail = f"Se ejecutarán {len(selected)} de {total} pruebas con arnés · {cost}."
            hidden = self._hidden_checked(self.harness_list)
            if hidden:
                detail += f" ({hidden} marcada(s) están ocultas por el filtro.)"
        self.harness_scope_label.setText(detail)
        self._update_plan_summary()

    @Slot()
    def _update_language_scope(self, *_args: object) -> None:
        total = self.language_list.count()
        selected = self._selected_language_values()
        if not selected:
            detail = f"Sin marcar nada: se ejecutarán las {total} pruebas de lenguaje de CTest."
        else:
            detail = f"Se ejecutarán {len(selected)} de {total} pruebas de lenguaje."
            hidden = self._hidden_checked(self.language_list)
            if hidden:
                detail += f" ({hidden} marcada(s) están ocultas por el filtro.)"
        # Aquí no hay nada que avisar sobre construcción: por definición, ninguna de estas
        # pruebas necesita un ejecutable de C++. Las que lo necesitaban están en «con arnés».
        self.language_scope_label.setText(detail)
        self._update_plan_summary()

    @Slot()
    def _update_case_scope(self, *_args: object) -> None:
        total = self.case_list.count()
        selected = self._selected_case_values()
        if not selected:
            detail = f"Sin marcar nada: se ejecutarán los {total} fixtures y ejemplos."
        else:
            detail = f"Se ejecutarán {len(selected)} de {total} fixtures y ejemplos."
            hidden = self._hidden_checked(self.case_list)
            if hidden:
                detail += f" ({hidden} marcado(s) están ocultos por el filtro.)"
        self.case_scope_label.setText(detail)
        self._update_plan_summary()

    @Slot()
    def _start_compiler(self) -> None:
        self.start_run("compiler")

    @Slot()
    def _start_install(self) -> None:
        self.start_run("install")

    @Slot()
    def _start_tests(self) -> None:
        """El botón grande de la pestaña del lenguaje: lo marcado, o todo si no hay nada."""
        self.start_run("language")

    @Slot()
    def _start_unit(self) -> None:
        self.start_run("unit")

    @Slot()
    def _start_harness(self) -> None:
        self.start_run("harness")

    def _run_selected_unit(self) -> None:
        # Nada marcado NO lanza las 291 desde este boton. En el plan «vacio» significa todas, y ahi
        # esta bien porque se eligio a proposito; aqui seria una sorpresa cara.
        if not self._selected_unit_values():
            QMessageBox.warning(
                self,
                APP_NAME,
                "Marca al menos una prueba unitaria.",
            )
            return
        self.start_run("unit")

    def _run_selected_harness(self) -> None:
        if not self._selected_harness_values():
            QMessageBox.warning(self, APP_NAME, "Marca al menos una prueba con arnés.")
            return
        self.stage_ctest.setChecked(True)
        self.stage_cases.setChecked(False)
        self.stage_triage.setChecked(False)
        self.start_run("harness")

    def _run_selected_language(self) -> None:
        if not self._selected_language_values():
            QMessageBox.warning(self, APP_NAME, "Marca al menos una prueba de lenguaje.")
            return
        self.stage_ctest.setChecked(True)
        self.stage_cases.setChecked(False)
        self.stage_triage.setChecked(False)
        self.start_run("language")

    def _run_selected_cases(self) -> None:
        if not self._selected_case_values():
            QMessageBox.warning(self, APP_NAME, "Marca al menos un fixture.")
            return
        self.stage_ctest.setChecked(False)
        self.stage_cases.setChecked(True)
        self.stage_triage.setChecked(False)
        self.start_run("language")

    def _update_plan_summary(self, *_args: object) -> None:
        if not hasattr(self, "compiler_summary") or not hasattr(self, "unit_list"):
            return
        compiler_mode = "desde cero" if self.build_mode.currentData() == "clean" else "incremental"
        self.compiler_summary.setText(
            f"nsc se construirá {compiler_mode} con {self.build_runner.currentText()} y "
            f"{self.compiler_jobs.value()} proceso(s), {self.host_optimization.currentText()} y "
            f"enlace {self.host_linkage.currentText()}; después se instalará. No se ejecutarán pruebas."
        )
        selected_ctests = self._selected_language_values()
        tests: list[str] = []
        if self.stage_ctest.isChecked():
            tests.append(
                "todas las pruebas de lenguaje de CTest"
                if not selected_ctests
                else f"{len(selected_ctests)} prueba(s) de lenguaje"
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
        test_mode = "sin compilar nada: solo el nsc instalado"
        optimizations = [name for checked, name in (
            (self.opt_o0.isChecked(), "O0"),
            (self.opt_o1.isChecked(), "O1"),
            (self.opt_o2.isChecked(), "O2"),
            (self.opt_o3.isChecked(), "O3"),
        ) if checked]
        selected_linkers = list(self.linker_mode.currentData())
        build_paths = {
            "lld-link": self.test_build_lld_path.text().strip(),
            "NSLink": self.test_build_nslink_path.text().strip(),
        }
        selected_build_text = " · ".join(
            f"{'LLD-link' if linker == 'lld-link' else linker}: {build_paths[linker]}"
            for linker in selected_linkers
        )
        self.test_summary.setText(
            f"{test_text} · {test_mode} · {self.linker_mode.currentText()} · "
            f"{', '.join(optimizations) or 'sin optimización'} · "
            f"{self.test_jobs.value()} proceso(s).\n"
            f"Árboles independientes: {selected_build_text}. El nsc instalado no se modifica."
        )

    def _load_settings(self) -> None:
        self.build_path.setText(self.settings.value("build_path", str(DEFAULT_BUILD)))
        self.test_build_lld_path.setText(
            self.settings.value("test_build_lld_path", str(DEFAULT_TEST_BUILD_LLD))
        )
        self.test_build_nslink_path.setText(
            self.settings.value("test_build_nslink_path", str(DEFAULT_TEST_BUILD_NSLINK))
        )
        self.prefix_path.setText(self.settings.value("prefix_path", str(DEFAULT_PREFIX)))
        self.llvm_path.setText(self.settings.value("llvm_path", str(DEFAULT_LLVM)))
        self.test_prefix_path.setText(
            self.settings.value("test_prefix_path", str(DEFAULT_PREFIX))
        )
        self.test_llvm_path.setText(
            self.settings.value("test_llvm_path", str(DEFAULT_LLVM))
        )
        saved_nslink = str(self.settings.value("nslink_path", str(DEFAULT_NSLINK)))
        legacy_nslink = str(DEFAULT_PREFIX / "bin" / "nslink.exe")
        if Path(saved_nslink) == Path(legacy_nslink):
            saved_nslink = str(DEFAULT_NSLINK)
        self.nslink_path.setText(saved_nslink)
        old_jobs = int(self.settings.value("jobs", min(os.cpu_count() or 4, 16)))
        self.compiler_jobs.setValue(int(self.settings.value("compiler_jobs", old_jobs)))
        self.test_jobs.setValue(int(self.settings.value("test_jobs", old_jobs)))
        build_mode_index = self.build_mode.findData(self.settings.value("build_mode", "incremental"))
        if build_mode_index >= 0:
            self.build_mode.setCurrentIndex(build_mode_index)
        runner_index = self.build_runner.findData(self.settings.value("build_runner", "cmake"))
        if runner_index >= 0:
            self.build_runner.setCurrentIndex(runner_index)
        linkage_index = self.host_linkage.findData(
            self.settings.value("host_linkage", "STATIC")
        )
        if linkage_index >= 0:
            self.host_linkage.setCurrentIndex(linkage_index)
        optimization_index = self.host_optimization.findData(
            self.settings.value("host_optimization", "O2")
        )
        if optimization_index >= 0:
            self.host_optimization.setCurrentIndex(optimization_index)
        test_runner_index = self.test_build_runner.findData(
            self.settings.value("test_build_runner", "cmake")
        )
        if test_runner_index >= 0:
            self.test_build_runner.setCurrentIndex(test_runner_index)
        linker_index = int(self.settings.value("linker_index", 0))
        if 0 <= linker_index < self.linker_mode.count():
            self.linker_mode.setCurrentIndex(linker_index)
        operation_tab_index = int(self.settings.value("operation_tab_index", 0))
        if 0 <= operation_tab_index < self.operation_tabs.count():
            self.operation_tabs.setCurrentIndex(operation_tab_index)
        geometry = self.settings.value("window_geometry")
        if geometry is not None:
            self.restoreGeometry(geometry)
        dock_state = self.settings.value("dock_state")
        if dock_state is not None:
            self.restoreState(dock_state, 1)

    def _save_settings(self) -> None:
        self.settings.setValue("build_path", self.build_path.text())
        self.settings.setValue("test_build_lld_path", self.test_build_lld_path.text())
        self.settings.setValue("test_build_nslink_path", self.test_build_nslink_path.text())
        self.settings.setValue("prefix_path", self.prefix_path.text())
        self.settings.setValue("llvm_path", self.llvm_path.text())
        self.settings.setValue("test_prefix_path", self.test_prefix_path.text())
        self.settings.setValue("test_llvm_path", self.test_llvm_path.text())
        self.settings.setValue("nslink_path", self.nslink_path.text())
        self.settings.setValue("compiler_jobs", self.compiler_jobs.value())
        self.settings.setValue("test_jobs", self.test_jobs.value())
        self.settings.setValue("build_mode", self.build_mode.currentData())
        self.settings.setValue("build_runner", self.build_runner.currentData())
        self.settings.setValue("host_linkage", self.host_linkage.currentData())
        self.settings.setValue("host_optimization", self.host_optimization.currentData())
        self.settings.setValue("test_build_runner", self.test_build_runner.currentData())
        self.settings.setValue("linker_index", self.linker_mode.currentIndex())
        self.settings.setValue("operation_tab_index", self.operation_tabs.currentIndex())
        self.settings.setValue("window_geometry", self.saveGeometry())
        self.settings.setValue("dock_state", self.saveState(1))

    def refresh_ctests(self) -> None:
        if self.inventory_process is not None:
            return
        builds = {
            "lld-link": Path(self.test_build_lld_path.text().strip()),
            "NSLink": Path(self.test_build_nslink_path.text().strip()),
        }
        selected_linkers = list(self.linker_mode.currentData())
        candidates = [builds[name] for name in selected_linkers]
        build = next(
            (candidate for candidate in candidates if (candidate / "CTestTestfile.cmake").is_file()),
            candidates[0],
        )
        if not (build / "CTestTestfile.cmake").is_file():
            self.status_label.setText(f"CTest: {build.name} aún no está configurado")
            return
        self.status_label.setText(f"Leyendo CTest desde {build.name}...")
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
        for test in tests:
            name = str(test["name"])
            command = test.get("command", [])
            if command:
                executable = Path(str(command[0]))
                # Sólo los ejecutables físicos de los árboles CTest son objetivos construibles aquí.
                # El `nsc.exe` instalado es el compilador BAJO prueba, nunca un target de pruebas.
                if executable.suffix.casefold() == ".exe" and executable.stem.startswith("nsc_"):
                    self.ctest_targets[name] = executable.stem
                else:
                    # CONOCIDA, PERO SIN OBJETIVO PROPIO. Es el `nsc.exe` instalado --el compilador
                    # BAJO prueba, nunca un target de pruebas--. Registrarla con cadena vacia es
                    # distinto de no registrarla: dice «se cual es y no hay que construir nada»,
                    # mientras que la ausencia significa «no se que necesita».
                    self.ctest_targets[name] = ""
        # CADA PRUEBA A SU PESTANA. Las que miden el compilador por dentro a «unitarias»; las que
        # lo usan desde fuera a «lenguaje». Ver `classify_ctest`.
        mapping = ctest_targets_from_cmake(ROOT)
        mapping.update(self.ctest_targets)
        buckets: dict[str, list[str]] = {CTEST_UNIT: [], CTEST_HARNESS: [], CTEST_LANGUAGE: []}
        for name in names:
            buckets[classify_ctest(name, mapping)].append(name)
        # Lo que ya estaba marcado se conserva al actualizar el inventario: perder la seleccion
        # cada vez que se pulsa «Actualizar» obligaria a rehacerla entera.
        for kind, item_list, group_list, groups in (
            (CTEST_UNIT, self.unit_list, self.unit_group_list, self.unit_groups),
            (CTEST_HARNESS, self.harness_list, self.harness_group_list, self.harness_groups),
            (CTEST_LANGUAGE, self.language_list, self.language_group_list, self.language_groups),
        ):
            groups.clear()
            for name in buckets[kind]:
                groups.setdefault(ctest_group(name), []).append(name)
            self._fill_selection(item_list, group_list, buckets[kind], groups)
        self.status_label.setText(
            f"CTest: {len(buckets[CTEST_UNIT])} unitarias · "
            f"{len(buckets[CTEST_HARNESS])} con arnés · "
            f"{len(buckets[CTEST_LANGUAGE])} de lenguaje")
        self._refresh_selection()

    def _fill_selection(
        self,
        item_list: QListWidget,
        group_list: QListWidget,
        names: list[str],
        groups: dict[str, list[str]],
    ) -> None:
        previous = set(self._checked_values(item_list))
        self._selection_guard = True
        try:
            item_list.clear()
            group_list.clear()
            for name in names:
                item = self._checkable(name, name)
                if name in previous:
                    item.setCheckState(Qt.CheckState.Checked)
                item_list.addItem(item)
            for group in sorted(groups, key=str.casefold):
                members = sorted(groups[group], key=str.casefold)
                groups[group] = members
                group_list.addItem(self._checkable(f"{group} (0/{len(members)})", group))
        finally:
            self._selection_guard = False

    def _targets_for_ctests(self, selected_ctests: list[str]) -> str:
        """El calculo puro, sin mirar si la etapa CTest esta marcada.

        Existe aparte porque el resumen de la pestana lo necesita para decir cuanto va a costar la
        seleccion ANTES de lanzar nada, y ahi la casilla de la etapa no pinta: mezclarlas hacia que
        el resumen dijera «construira todos» solo por tener CTest desmarcado.
        """
        known = ctest_targets_from_cmake(ROOT)
        known.update(self.ctest_targets)
        targets: list[str] = []
        for name in selected_ctests:
            target = ctest_target_for(name, known)
            if target is None:
                return "pruebas"
            if target:
                targets.append(target)
        # Vacio a proposito: la seleccion no necesita ningun ejecutable de prueba. Se configura si
        # hace falta, pero no se construye nada.
        return ";".join(dict.fromkeys(targets))

    # ------------------------------------------------------------------ Gate 0 del plan 123

    def _abi_page(self) -> QWidget:
        page = QWidget()
        layout = QVBoxLayout(page)
        explanation = QLabel(
            "Gate 0 del plan 123. Preserva el estado verde actual antes de tocar el compilador.\n\n"
            "G0.1 archiva el arbol INSTALADO tal como esta ahora mismo, junto a los ultimos logs "
            "verdes. Es evidencia historica: no certifica nada por si sola.\n\n"
            "G0.2 mide el arbol y los binarios, construye, instala, ejecuta la matriz completa "
            "--CTest, fixtures y bug triage, con los dos enlazadores y O0/O1/O2/O3--, vuelve a medir y "
            "exige que nada se haya movido. Solo entonces escribe el baseline certificado."
        )
        explanation.setWordWrap(True)
        layout.addWidget(explanation)

        historical = QGroupBox("G0.1 — archivo historico")
        historical_layout = QVBoxLayout(historical)
        self.abi_archive_button = QPushButton("Archivar el arbol instalado ahora")
        self.abi_archive_button.clicked.connect(self._archive_historical)
        historical_layout.addWidget(self.abi_archive_button)
        self.abi_archive_status = QLabel("Sin archivar en esta sesion.")
        self.abi_archive_status.setWordWrap(True)
        historical_layout.addWidget(self.abi_archive_status)
        layout.addWidget(historical)

        certified = QGroupBox("G0.2 — baseline certificado")
        certified_layout = QVBoxLayout(certified)
        certified_note = QLabel(
            "Usa las rutas de la pestana Compilador. La matriz no es configurable: un baseline "
            "parcial no es un baseline. La preparación es incremental y conserva los tres "
            "árboles de build; las pruebas sí se ejecutan completas porque una certificación no "
            "puede reutilizar resultados anteriores."
        )
        certified_note.setWordWrap(True)
        certified_layout.addWidget(certified_note)
        self.abi_baseline_button = QPushButton("Construir, medir y certificar el baseline")
        self.abi_baseline_button.clicked.connect(self._start_baseline)
        certified_layout.addWidget(self.abi_baseline_button)
        self.baseline_clean_button = QPushButton("Clean")
        self.baseline_clean_button.setToolTip(
            "Elimina builds, ejecutables de pruebas, temporales y error*.log; conserva evidencia"
        )
        self.baseline_clean_button.setEnabled(False)
        self.baseline_clean_button.clicked.connect(self.start_cleanup)
        certified_layout.addWidget(self.baseline_clean_button)
        layout.addWidget(certified)
        layout.addStretch(1)
        return page

    def _last_green_run(self) -> Path | None:
        """La corrida verde mas reciente del historial, o None si no hay ninguna."""
        if not HISTORY_ROOT.is_dir():
            return None
        candidates: list[tuple[str, Path]] = []
        for directory in HISTORY_ROOT.iterdir():
            metadata = directory / "metadata.json"
            if not metadata.is_file():
                continue
            try:
                payload = json.loads(metadata.read_text(encoding="utf-8", errors="replace"))
            except (OSError, ValueError):
                continue
            if payload.get("status") == "passed" and not payload.get("failed"):
                candidates.append((str(payload.get("finished_at") or directory.name), directory))
        if not candidates:
            return None
        candidates.sort()
        return candidates[-1][1]

    @Slot()
    def _archive_historical(self) -> None:
        """G0.1. Copia el arbol instalado tal como esta. Nunca sobrescribe, nunca borra."""
        if self.worker is not None:
            QMessageBox.warning(self, APP_NAME, "Hay una corrida en curso.")
            return
        prefix = Path(self.prefix_path.text().strip())
        if not (prefix / "bin" / "nsc.exe").is_file():
            QMessageBox.warning(
                self, APP_NAME, f"No hay un compilador instalado en:\n{prefix}"
            )
            return
        destination = ABI_ARCHIVES_ROOT / "historical" / datetime.now().strftime("%Y%m%d-%H%M%S")
        if destination.exists():
            QMessageBox.warning(self, APP_NAME, f"Ya existe:\n{destination}")
            return
        answer = QMessageBox.question(
            self,
            APP_NAME,
            f"Se copiara el arbol instalado\n{prefix}\n\na\n{destination}\n\n"
            "No se borra ni se modifica nada. ¿Continuar?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
            QMessageBox.StandardButton.No,
        )
        if answer != QMessageBox.StandardButton.Yes:
            return
        try:
            payload_root = destination / "prefix"
            shutil.copytree(prefix, payload_root)
            entries = canonical_manifest(payload_root, collect_tree_files(payload_root))
            green = self._last_green_run()
            # El plan exige decir en voz alta que esto NO es un baseline certificado: los logs
            # verdes del historial no contienen el hash del binario que los produjo, asi que
            # asociarlos es una pista, no una prueba.
            record = {
                "gate": "G00",
                "part": "G0.1",
                "kind": "historical-archive",
                "certified_baseline": False,
                "reason": (
                    "Los logs verdes del historial no registran el hash del nsc.exe que los "
                    "produjo, de modo que la asociacion es evidencia historica y no un baseline "
                    "certificado. El baseline lo produce G0.2."
                ),
                "created_at": utc_stamp(),
                "prefix": str(prefix.resolve()),
                "destination": str(destination.resolve()),
                "installed_nsc_sha256": sha256_file(prefix / "bin" / "nsc.exe"),
                "file_count": len(entries),
                "manifest_digest": manifest_digest(entries),
                "green_run": str(green.resolve()) if green is not None else None,
            }
            write_json(destination / "manifest.json", {"record": record, "files": entries})
            write_json(ABI_GATES_ROOT / "G00" / "historical.json", record)
        except (OSError, ValueError) as exc:
            QMessageBox.critical(self, APP_NAME, f"No se pudo archivar:\n{exc!r}")
            self.abi_archive_status.setText(f"Fallo: {exc!r}")
            return
        self.abi_archive_status.setText(
            f"Archivado: {destination}\n{record['file_count']} ficheros, "
            f"digest {record['manifest_digest'][:16]}…\n"
            "No es un baseline certificado: eso lo produce G0.2."
        )
        QMessageBox.information(self, APP_NAME, f"Archivo historico creado en:\n{destination}")

    @Slot()
    def _start_baseline(self) -> None:
        answer = QMessageBox.question(
            self,
            APP_NAME,
            "El Gate 0 construira, instalara y ejecutara la matriz completa con los dos "
            "enlazadores y O0/O1/O2/O3, y despues exigira que fuentes y binarios no se hayan "
            "movido.\n\n"
            "¿Continuar?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
            QMessageBox.StandardButton.No,
        )
        if answer != QMessageBox.StandardButton.Yes:
            return
        self.start_run("baseline")

    def _make_plan(self, operation: str) -> RunPlan | None:
        compiler_build = Path(self.build_path.text().strip()).resolve()
        test_build_dirs = {
            "lld-link": self.test_build_lld_path.text().strip(),
            "NSLink": self.test_build_nslink_path.text().strip(),
        }
        resolved_test_builds = {
            linker: Path(path).resolve() for linker, path in test_build_dirs.items()
        }
        if compiler_build in resolved_test_builds.values():
            QMessageBox.warning(
                self,
                APP_NAME,
                "El compilador no puede compartir directorio con ninguno de los árboles de CTest.",
            )
            return None
        if operation in ("compiler", "install"):
            if compiler_build.name.casefold().startswith("build-test"):
                QMessageBox.warning(
                    self,
                    APP_NAME,
                    "El directorio del compilador no puede ser un árbol build-test-*.",
                )
                return None
            if operation == "install" and not (compiler_build / "cmake_install.cmake").is_file():
                QMessageBox.warning(
                    self,
                    APP_NAME,
                    "El build seleccionado no está configurado para instalar:\n"
                    f"{compiler_build}\n\nFalta cmake_install.cmake.",
                )
                return None
            return RunPlan(
                operation=operation,
                root=str(ROOT),
                compiler_build_dir=self.build_path.text().strip(),
                prefix=self.prefix_path.text().strip(),
                deploy_prefix=self.prefix_path.text().strip(),
                llvm_root=self.llvm_path.text().strip(),
                nslink_exe=self.nslink_path.text().strip(),
                history_root=str(HISTORY_ROOT),
                test_work_root=str(DEFAULT_TEST_WORK_ROOT),
                temp_root=str(DEFAULT_TEMP_ROOT),
                allowed_write_roots=[],
                run_id="",
                # La pestaña del compilador produce un binario distribuible: identidad nueva.
                # Los baselines reproducibles la fijan explícitamente con --build-id/config.
                build_id_override="",
                host_linkage=str(self.host_linkage.currentData()),
                host_optimization=str(self.host_optimization.currentData()),
                build_mode=(
                    "skip" if operation == "install" else str(self.build_mode.currentData())
                ),
                build_target="nsc",
                build_runner=str(self.build_runner.currentData()),
                # La pestaña del compilador construye nsc, y ya. No toca los árboles de prueba.
                build_test_trees=False,
                test_build_target="",
                test_build_mode="skip",
                test_build_dirs={},
                linkers=[],
                optimizations=[],
                jobs=self.compiler_jobs.value(),
                run_build=True,
                sync_libraries=False,
                run_ctest=False,
                run_cases=False,
                run_triage=False,
                keep_link_temps=False,
                selected_ctests=[],
                selected_cases=[],
            )
        if not any(
            (self.stage_ctest.isChecked(), self.stage_cases.isChecked(), self.stage_triage.isChecked())
        ):
            QMessageBox.warning(self, APP_NAME, "Selecciona por lo menos un conjunto de pruebas.")
            return None
        optimizations = []
        if self.opt_o0.isChecked():
            optimizations.append("-O0")
        if self.opt_o1.isChecked():
            optimizations.append("-O1")
        if self.opt_o2.isChecked():
            optimizations.append("-O2")
        if self.opt_o3.isChecked():
            optimizations.append("-O3")
        if operation == "language" and self.stage_cases.isChecked() and not optimizations:
            QMessageBox.warning(self, APP_NAME, "Selecciona por lo menos uno de O0, O1, O2 y O3.")
            return None
        selected_ctests = {
            "unit": self._selected_unit_values,
            "harness": self._selected_harness_values,
        }.get(operation, self._selected_language_values)()
        selected_cases = self._selected_case_values() if operation == "language" else []
        linkers = list(self.linker_mode.currentData())
        selected_builds = [resolved_test_builds[linker] for linker in linkers]
        if len(set(selected_builds)) != len(selected_builds):
            QMessageBox.warning(
                self,
                APP_NAME,
                "LLD-link y NSLink deben usar directorios CTest diferentes.",
            )
            return None
        if operation == "baseline":
            # G0.2 no es configurable a proposito. Un baseline parcial --un solo enlazador, un solo
            # nivel de optimizacion, sin fixtures-- seria un certificado que no dice lo que parece
            # decir, y el Gate 0 existe justamente para que no exista esa clase de verde.
            return make_baseline_plan(
                self.test_jobs.value(),
                compiler_build_dir=self.build_path.text().strip(),
                prefix=DEFAULT_BASELINE_PREFIX,
                deploy_prefix=self.prefix_path.text().strip(),
                llvm_root=self.llvm_path.text().strip(),
                nslink_exe=self.nslink_path.text().strip(),
                history_root=str(HISTORY_ROOT),
                test_work_root=str(DEFAULT_TEST_WORK_ROOT),
                temp_root=str(DEFAULT_TEMP_ROOT),
                allowed_write_roots=[],
                run_id="",
                build_id_override="",
                test_build_lld=test_build_dirs["lld-link"],
                test_build_nslink=test_build_dirs["NSLink"],
                build_runner=str(self.build_runner.currentData()),
            )
        return RunPlan(
            operation=operation,
            root=str(ROOT),
            compiler_build_dir="",
            prefix=self.test_prefix_path.text().strip(),
            deploy_prefix=self.test_prefix_path.text().strip(),
            llvm_root=self.test_llvm_path.text().strip(),
            nslink_exe=self.nslink_path.text().strip(),
            history_root=str(HISTORY_ROOT),
            test_work_root=str(DEFAULT_TEST_WORK_ROOT),
            temp_root=str(DEFAULT_TEMP_ROOT),
            allowed_write_roots=[],
            run_id="",
            build_id_override="",
            host_linkage="STATIC",
            host_optimization="O2",
            build_mode="skip",
            build_target="",
            build_runner=str(self.test_build_runner.currentData()),
            # CADA CONJUNTO CONSTRUYE LO SUYO Y NADA MAS.
            #   unit    -- sus ejecutables.
            #   harness -- solo el arnes que comparten.
            #   language-- nada, jamas: solo el `nsc.exe` instalado.
            build_test_trees=operation in ("unit", "harness"),
            test_build_target=(
                self._targets_for_ctests(selected_ctests)
                if operation in ("unit", "harness") and selected_ctests
                else ("pruebas" if operation == "unit" else "")
            ),
            test_build_mode="incremental" if operation in ("unit", "harness") else "skip",
            test_build_dirs={linker: test_build_dirs[linker] for linker in linkers},
            linkers=linkers,
            optimizations=optimizations,
            jobs=self.test_jobs.value(),
            run_build=False,
            sync_libraries=True,
            # Las casillas de conjuntos son de la pestaña del lenguaje. Unitarias y arnés
            # ejecutan CTest y solo CTest: mezclarles fixtures sería el cruce que se quiere evitar.
            run_ctest=(
                self.stage_ctest.isChecked() if operation == "language" else True
            ),
            run_cases=self.stage_cases.isChecked() and operation == "language",
            run_triage=self.stage_triage.isChecked() and operation == "language",
            keep_link_temps=self.keep_temps.isChecked(),
            selected_ctests=selected_ctests,
            selected_cases=selected_cases,
        )

    def start_run(self, operation: str) -> None:
        if self.worker is not None:
            return
        plan = self._make_plan(operation)
        if plan is None:
            return
        if not any((plan.run_build, plan.sync_libraries, plan.run_ctest, plan.run_cases, plan.run_triage)):
            QMessageBox.warning(self, APP_NAME, "Selecciona por lo menos una etapa.")
            return
        if plan.build_mode == "clean" and plan.run_build:
            answer = QMessageBox.question(
                self,
                APP_NAME,
                f"Se borrará por completo:\n{plan.compiler_build_dir}\n\n¿Continuar?",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
                QMessageBox.StandardButton.No,
            )
            if answer != QMessageBox.StandardButton.Yes:
                return
        if plan.operation == "tests" and plan.test_build_mode == "clean":
            clean_targets: list[Path] = []
            if plan.run_ctest:
                clean_targets.extend(
                    Path(plan.test_build_dirs[linker]).resolve() for linker in plan.linkers
                )
            if plan.run_cases or plan.run_triage:
                clean_targets.extend(
                    (TEST_WORK_ROOT / safe_name(linker)).resolve() for linker in plan.linkers
                )
            targets = "\n".join(str(path) for path in clean_targets)
            answer = QMessageBox.question(
                self,
                APP_NAME,
                f"Se borrarán los artefactos anteriores de estas pruebas:\n{targets}\n\n¿Continuar?",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
                QMessageBox.StandardButton.No,
            )
            if answer != QMessageBox.StandardButton.Yes:
                return
        self._save_settings()
        test_channels = ["LLD-link" if linker == "lld-link" else linker for linker in plan.linkers]
        if plan.operation in ("compiler", "install"):
            self.active_result_channels = ["Compilador"]
        elif plan.operation == "baseline":
            # El Gate 0 construye el compilador Y ejecuta la matriz en la misma corrida, asi que
            # produce mensajes de las dos clases y necesita los dos canales.
            self.active_result_channels = ["Compilador", *test_channels]
        else:
            self.active_result_channels = test_channels
        channels_to_clear = self.active_result_channels
        for channel in dict.fromkeys(channels_to_clear):
            channel_optimizations = (
                ("General",) if channel == "Compilador" else self.output_optimizations
            )
            for optimization in channel_optimizations:
                key = (channel, optimization)
                self.channel_consoles[key].clear()
            self.channel_error_tables[channel].setRowCount(0)
            self.channel_error_counts[channel] = 0
            tabs = self.channel_output_tabs[channel]
            tabs.setTabText(self.channel_error_tab_indices[channel], "Errores (0)")
            tabs.setTabText(self.channel_results_tab_indices[channel], "Resultados")
            tabs.setCurrentIndex(0)
            self.output_channel_tabs.setTabText(
                self.channel_outer_indices[channel], self.channel_titles[channel]
            )
            self.channel_optimization_tabs[channel].setCurrentIndex(
                self.channel_optimization_indices[(channel, "General")]
            )
        initial_channel = (
            "LLD-link" if plan.linkers and plan.linkers[0] == "lld-link" else plan.linkers[0]
        ) if plan.operation == "tests" and plan.linkers else "Compilador"
        self.output_channel_tabs.setCurrentIndex(self.channel_outer_indices[initial_channel])
        for channel in self.active_result_channels:
            self.stage_tables[channel].setRowCount(0)
        self.stage_rows_by_name.clear()
        self.current_run_dir = None
        self.open_button.setEnabled(False)
        self.compiler_start_button.setEnabled(False)
        self.compiler_install_button.setEnabled(False)
        self.tests_start_button.setEnabled(False)
        self.abi_baseline_button.setEnabled(False)
        self.baseline_clean_button.setEnabled(False)
        self.abi_archive_button.setEnabled(False)
        self.cleanup_button.setEnabled(False)
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
    def start_cleanup(self) -> None:
        if self.worker is not None:
            return
        if self.inventory_process is not None:
            QMessageBox.information(
                self,
                APP_NAME,
                "Espera a que termine la lectura del inventario CTest antes de limpiar.",
            )
            return
        configured = [
            Path(self.build_path.text().strip()),
            Path(self.test_build_lld_path.text().strip()),
            Path(self.test_build_nslink_path.text().strip()),
        ]
        targets, rejected = cleanup_targets(configured)
        if rejected:
            paths = "\n".join(str(path) for path in rejected)
            QMessageBox.warning(
                self,
                APP_NAME,
                f"La limpieza sólo admite árboles build-* dentro de {ROOT}. "
                f"Corrige estas rutas antes de continuar:\n\n{paths}",
            )
            return
        if not targets:
            QMessageBox.information(self, APP_NAME, "No hay artefactos de compilación que limpiar.")
            return
        error_logs = [
            path
            for path in targets
            if path.parent == ROOT.resolve()
            and re.fullmatch(r"error(?:\d+)?\.log", path.name, re.IGNORECASE)
        ]
        displayed_targets = [path for path in targets if path not in error_logs]
        paths = "\n".join(f"• {path}" for path in displayed_targets)
        if error_logs:
            paths += f"\n• {len(error_logs)} archivo(s) error*.log en {ROOT.resolve()}"
        answer = QMessageBox.question(
            self,
            APP_NAME,
            "Se eliminarán permanentemente estos artefactos de compilación y pruebas:\n\n"
            f"{paths}\n\n"
            "Se conservarán _abi, _nscbuilder\\history, las fuentes y C:\\native-sharp.\n\n"
            "¿Continuar?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
            QMessageBox.StandardButton.No,
        )
        if answer != QMessageBox.StandardButton.Yes:
            return

        self._save_settings()
        self.active_result_channels = ["Compilador"]
        self.output_channel_tabs.setCurrentIndex(self.channel_outer_indices["Compilador"])
        self.compiler_start_button.setEnabled(False)
        self.compiler_install_button.setEnabled(False)
        self.tests_start_button.setEnabled(False)
        self.abi_baseline_button.setEnabled(False)
        self.baseline_clean_button.setEnabled(False)
        self.abi_archive_button.setEnabled(False)
        self.cleanup_button.setEnabled(False)
        self.cancel_button.setEnabled(True)
        self.status_label.setText("Limpiando artefactos...")

        thread = QThread(self)
        worker = CleanupWorker(targets)
        worker.moveToThread(thread)
        thread.started.connect(worker.run)
        worker.output.connect(self.append_output)
        worker.progress.connect(self.on_progress)
        worker.finished.connect(self.on_cleanup_finished)
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
        aliases = {"General": "Compilador", "lld-link": "LLD-link"}
        normalized = aliases.get(channel, channel)
        return normalized if normalized in self.output_channels else "Compilador"

    def _known_optimization(self, optimization: str) -> str:
        normalized = optimization.lstrip("-")
        return normalized if normalized in self.output_optimizations else "General"

    @Slot()
    def clear_current_console(self) -> None:
        index = self.output_channel_tabs.currentIndex()
        if index == self.history_outer_index:
            return
        channel = next(
            (
                name
                for name, channel_index in self.channel_outer_indices.items()
                if channel_index == index
            ),
            "Compilador",
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
        if channel == "Compilador":
            optimization = "General"
        key = (channel, optimization)
        console = self.channel_consoles[key]
        cursor = console.textCursor()
        cursor.movePosition(QTextCursor.MoveOperation.End)
        fmt = QTextCharFormat()
        fmt.setForeground(colors.get(level, colors["info"]))
        cursor.insertText(line + "\n", fmt)
        console.setTextCursor(cursor)
        console.ensureCursorVisible()

    @Slot(dict)
    def on_failure_found(self, failure: dict) -> None:
        channel = self._known_output_channel(str(failure.get("linker", "Compilador")))
        optimization = self._known_optimization(str(failure.get("optimization", "General")))
        error_table = self.channel_error_tables[channel]
        row = error_table.rowCount()
        error_table.insertRow(row)
        log_path = str(failure.get("error_log", ""))
        source = str(failure.get("source", ""))
        failure_set = str(failure.get("kind", ""))
        if not failure_set:
            if source.startswith("nsc."):
                failure_set = "CTest"
            elif re.match(r"b\d+_", source):
                failure_set = "Bug triage"
            elif re.search(r"(?:cc1_)?c\d+_", source):
                failure_set = "Fixtures"
            else:
                failure_set = "Infraestructura"
        values = (
            optimization,
            failure_set,
            source,
            str(failure.get("detail", "")),
            Path(log_path).name if log_path else "",
        )
        for column, value in enumerate(values):
            item = QTableWidgetItem(value)
            item.setData(Qt.ItemDataRole.UserRole, log_path)
            item.setToolTip(f"Abrir {log_path}" if log_path else "No hay log asociado")
            error_table.setItem(row, column, item)
        count = error_table.rowCount()
        self.channel_error_counts[channel] = count
        channel_tabs = self.channel_output_tabs[channel]
        error_index = self.channel_error_tab_indices[channel]
        channel_tabs.setTabText(error_index, f"Errores ({count})")
        channel_tabs.setCurrentIndex(error_index)
        self.output_channel_tabs.setTabText(
            self.channel_outer_indices[channel],
            f"{self.channel_titles[channel]} · {count} error(es)",
        )
        self.output_channel_tabs.setCurrentIndex(self.channel_outer_indices[channel])

    @Slot(QTableWidgetItem)
    def open_error_log(self, item: QTableWidgetItem) -> None:
        log_path = Path(str(item.data(Qt.ItemDataRole.UserRole) or ""))
        if log_path.is_file():
            QDesktopServices.openUrl(QUrl.fromLocalFile(str(log_path)))
            return
        QMessageBox.warning(self, APP_NAME, f"El log no existe:\n{log_path}")

    def _result_channels_for_stage(self, name: str) -> list[str]:
        if name.endswith("[lld-link]"):
            return ["LLD-link"]
        if name.endswith("[NSLink]"):
            return ["NSLink"]
        return list(self.active_result_channels)

    @Slot(str)
    def on_stage_started(self, name: str) -> None:
        rows: list[tuple[str, int]] = []
        for channel in self._result_channels_for_stage(name):
            table = self.stage_tables[channel]
            row = table.rowCount()
            table.insertRow(row)
            table.setItem(row, 0, QTableWidgetItem(name))
            table.setItem(row, 1, QTableWidgetItem("ejecutando"))
            table.setItem(row, 2, QTableWidgetItem(""))
            table.setItem(row, 3, QTableWidgetItem(""))
            self.channel_output_tabs[channel].setTabText(
                self.channel_results_tab_indices[channel],
                f"Resultados ({table.rowCount()})",
            )
            rows.append((channel, row))
        self.stage_rows_by_name[name] = rows
        self.status_label.setText(name)

    @Slot(str, str, float, int)
    def on_stage_finished(self, name: str, status: str, seconds: float, exit_code: int) -> None:
        rows = self.stage_rows_by_name.get(name)
        if not rows:
            return
        for channel, row in rows:
            table = self.stage_tables[channel]
            table.item(row, 1).setText(status)
            table.item(row, 2).setText(f"{seconds:.3f} s")
            table.item(row, 3).setText(str(exit_code))

    @Slot(int, int)
    def on_progress(self, value: int, total: int) -> None:
        self.progress_bar.setRange(0, max(total, 1))
        self.progress_bar.setValue(min(value, total))

    @Slot(dict)
    def on_finished(self, summary: dict) -> None:
        self.current_run_dir = Path(summary["run_dir"])
        self.baseline_cleanup_available = summary.get("operation") == "baseline"
        self.compiler_start_button.setEnabled(True)
        self.compiler_install_button.setEnabled(True)
        self.tests_start_button.setEnabled(True)
        self.abi_baseline_button.setEnabled(True)
        self.baseline_clean_button.setEnabled(self.baseline_cleanup_available)
        self.abi_archive_button.setEnabled(True)
        self.cleanup_button.setEnabled(True)
        self.cancel_button.setEnabled(False)
        self.open_button.setEnabled(True)
        self.status_label.setText(
            f"{summary['status']} — {summary['seconds']:.2f} s — {summary['failed']} fallo(s)"
        )
        if summary.get("operation") == "tests":
            QTimer.singleShot(0, self.refresh_ctests)
        QTimer.singleShot(0, self.refresh_history)

    @Slot(dict)
    def on_cleanup_finished(self, summary: dict) -> None:
        status = str(summary["status"])
        if status == "passed":
            self.baseline_cleanup_available = False
        self.compiler_start_button.setEnabled(True)
        self.compiler_install_button.setEnabled(True)
        self.tests_start_button.setEnabled(True)
        self.abi_baseline_button.setEnabled(True)
        self.baseline_clean_button.setEnabled(self.baseline_cleanup_available)
        self.abi_archive_button.setEnabled(True)
        self.cleanup_button.setEnabled(True)
        self.cancel_button.setEnabled(False)
        removed = int(summary["removed"])
        seconds = float(summary["seconds"])
        if status == "passed":
            self.status_label.setText(
                f"Limpieza completada — {removed} destino(s) — {seconds:.2f} s"
            )
        elif status == "cancelled":
            self.status_label.setText(f"Limpieza cancelada — {removed} destino(s) eliminados")
        else:
            failures = list(summary.get("failures", []))
            self.status_label.setText(
                f"Limpieza incompleta — {len(failures)} error(es) — {seconds:.2f} s"
            )
            QMessageBox.warning(
                self,
                APP_NAME,
                "No se pudieron eliminar algunos artefactos:\n\n" + "\n".join(failures),
            )
        self.ctest_targets.clear()

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


def run_baseline_cli(plan: RunPlan) -> int:
    """Ejecuta G0.2 sin crear una ventana y devuelve un código apto para automatización."""
    result: dict = {}
    last_line: list[str] = []
    if plan.run_id and safe_name(plan.run_id) != plan.run_id:
        print(f"--run-id contiene caracteres inseguros: {plan.run_id!r}", file=sys.stderr)
        return 2
    if plan.run_id and (Path(plan.history_root) / plan.run_id).exists():
        print(f"--run-id ya existe y no puede reutilizarse: {plan.run_id}", file=sys.stderr)
        return 2
    rejected = validate_plan_write_paths(plan)
    if rejected:
        print("El baseline fue rechazado antes de escribir: destinos fuera de --write-root:", file=sys.stderr)
        for path in rejected:
            print(f"  {path}", file=sys.stderr)
        return 2
    worker = PipelineWorker(plan, import_case_table(ROOT))

    def print_output(line: str, _level: str, _channel: str, _optimization: str) -> None:
        # Los mensajes generales se publican en los dos canales de la GUI. En consola se imprime
        # una sola vez cada duplicado consecutivo sin alterar los logs canónicos del runner.
        if last_line and last_line[0] == line:
            return
        last_line[:] = [line]
        print(line, flush=True)

    def print_stage(name: str, status: str, seconds: float, exit_code: int) -> None:
        print(f"==== {status.upper():9} {name} ({seconds:.3f} s, exit {exit_code})", flush=True)

    worker.output.connect(print_output)
    worker.stage_finished.connect(print_stage)
    worker.finished.connect(result.update)
    worker.run()

    report = result.get("report")
    if report:
        print(f"Reporte: {report}", flush=True)
    return 0 if result.get("status") == "passed" else 1


def main() -> int:
    config_parser = argparse.ArgumentParser(add_help=False)
    config_parser.add_argument("--config")
    config_args, _ = config_parser.parse_known_args()
    try:
        config = load_builder_config(config_args.config)
        build_id_default = configured_build_id(config)
    except (OSError, json.JSONDecodeError, ValueError) as exc:
        print(f"No se pudo cargar --config: {exc}", file=sys.stderr)
        return 2

    prefix_default = configured_path(config, "prefix", DEFAULT_PREFIX)
    baseline_prefix_default = configured_path(
        config, "baseline_prefix", DEFAULT_BASELINE_PREFIX
    )
    build_default = configured_path(config, "build_dir", DEFAULT_BUILD)
    llvm_default = configured_path(config, "llvm_root", DEFAULT_LLVM)
    nslink_default = configured_path(
        config,
        "nslink",
        DEFAULT_NSLINK,
    )
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--help", action="store_true")
    parser.add_argument("--baseline", action="store_true")
    parser.add_argument("--jobs", type=int, default=min(os.cpu_count() or 4, 16))
    parser.add_argument("--config")
    parser.add_argument("--prefix", default=prefix_default)
    parser.add_argument("--baseline-prefix", default=baseline_prefix_default)
    parser.add_argument("--build-dir", default=build_default)
    parser.add_argument(
        "--test-build-lld",
        default=configured_path(config, "test_build_lld", DEFAULT_TEST_BUILD_LLD),
    )
    parser.add_argument(
        "--test-build-nslink",
        default=configured_path(config, "test_build_nslink", DEFAULT_TEST_BUILD_NSLINK),
    )
    parser.add_argument("--llvm-root", default=llvm_default)
    parser.add_argument("--nslink", default=nslink_default)
    parser.add_argument(
        "--history-root",
        default=configured_path(config, "history_root", HISTORY_ROOT),
    )
    parser.add_argument(
        "--work-root",
        default=configured_path(config, "work_root", DEFAULT_TEST_WORK_ROOT),
    )
    parser.add_argument(
        "--temp-root",
        default=configured_path(config, "temp_root", DEFAULT_TEMP_ROOT),
    )
    parser.add_argument("--write-root", action="append")
    parser.add_argument("--run-id", default="")
    parser.add_argument("--build-id", default=build_id_default)
    args, qt_args = parser.parse_known_args()
    if args.help:
        print(__doc__)
        print(
            "Opciones de rutas para --baseline:\n"
            "  --config FILE             JSON nscbuilder-paths v1\n"
            "  --prefix DIR              instalación de NativeSharp\n"
            "  --baseline-prefix DIR     staging certificado del baseline\n"
            "  --build-dir DIR           build del compilador\n"
            "  --test-build-lld DIR      build CTest con lld-link\n"
            "  --test-build-nslink DIR   build CTest con NSLink\n"
            "  --llvm-root DIR           instalación de LLVM\n"
            "  --nslink FILE             ejecutable NSLink\n"
            "  --history-root DIR        logs y reportes\n"
            "  --work-root DIR           artefactos de fixtures\n"
            "  --temp-root DIR           TEMP/TMP de procesos hijos\n"
            "  --write-root DIR          raíz autorizada; repetible\n\n"
            "  --run-id ID               identidad estable para evidencia automatizada\n\n"
            "  --build-id HEX16          identidad reproducible para nsc0\n\n"
            "Entorno equivalente: NSC_INSTALL_ROOT, NSC_BASELINE_ROOT, NSC_BUILD_ROOT, "
            "NSC_TEST_BUILD_LLD,\n"
            "NSC_TEST_BUILD_NSLINK, NSC_LLVM_ROOT, NSLINK_EXE, NSC_HISTORY_ROOT,\n"
            "NSC_WORK_ROOT, NSC_TEMP_ROOT y NSC_WRITE_ROOTS."
        )
        return 0
    if args.baseline:
        if qt_args:
            print(f"Argumentos no reconocidos para --baseline: {' '.join(qt_args)}", file=sys.stderr)
            return 2
        if not 1 <= args.jobs <= 128:
            print("--jobs debe estar entre 1 y 128", file=sys.stderr)
            return 2
        write_roots = args.write_root if args.write_root is not None else configured_write_roots(config)
        plan = make_baseline_plan(
            args.jobs,
            compiler_build_dir=args.build_dir,
            prefix=args.baseline_prefix,
            deploy_prefix=args.prefix,
            llvm_root=args.llvm_root,
            nslink_exe=args.nslink,
            test_build_lld=args.test_build_lld,
            test_build_nslink=args.test_build_nslink,
            history_root=args.history_root,
            test_work_root=args.work_root,
            temp_root=args.temp_root,
            allowed_write_roots=write_roots,
            run_id=args.run_id,
            build_id_override=args.build_id,
        )
        return run_baseline_cli(plan)
    app = QApplication([sys.argv[0], *qt_args])
    app.setApplicationName(APP_NAME)
    app.setStyle("Fusion")
    app.setStyleSheet(dark_stylesheet())
    window = MainWindow()
    window.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
