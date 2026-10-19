"""Extracted gui implementation for modular NSC Builder 3."""

from __future__ import annotations

import argparse
import csv
import hashlib
import importlib.util
import json
import math
import os
import queue
import re
import shutil
import signal
import subprocess
import sys
import threading
import time
import uuid
from collections import Counter, defaultdict
from dataclasses import asdict, dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable, Iterable, Sequence

from .constants import *
from .paths import *
from .models import *
from .utils import *
from .configuration import *
from .catalog import *
from .history import *
from .reporting import *
from .process_control import *
from .build import *
from .execution import *
from .runtime_support import *


def _smoke_catalog() -> CanonicalCatalog:
    """Return a deterministic in-memory catalog for structural GUI checks."""
    case = CaseRecord(
        case_id="SMOKE-UNIT-001",
        contract_id="SMOKE-GUI-001",
        component="Smoke.GUI",
        wave=0,
        purpose="Exercise the unit-test projection without repository metadata.",
        layer="unit",
        profile="PR",
        build_modes=("Debug",),
        optimizations=("NA",),
        executor="nsc_component_tests",
        disposition="Required",
        objective="Construct every top-level workspace.",
        expected_outcome="PASS",
        manifest={},
    )
    return CanonicalCatalog((case,))


def launch_gui(smoke_only: bool = False) -> int:
    """Launch the complete single-file PySide6 application."""
    try:
        from PySide6.QtCore import Qt, QThread, QTimer, QUrl, Signal
        from PySide6.QtGui import QColor, QDesktopServices
        from PySide6.QtWidgets import (
            QAbstractItemView,
            QApplication,
            QComboBox,
            QFileDialog,
            QFormLayout,
            QGridLayout,
            QGroupBox,
            QHBoxLayout,
            QHeaderView,
            QLabel,
            QLineEdit,
            QMainWindow,
            QMenu,
            QMessageBox,
            QProgressBar,
            QPlainTextEdit,
            QPushButton,
            QSpinBox,
            QTabWidget,
            QTableWidget,
            QTableWidgetItem,
            QTextEdit,
            QVBoxLayout,
            QWidget,
        )
    except ImportError as error:
        raise RuntimeError("NSC Builder 3 requires PySide6 for its graphical interface") from error

    from .baseline_page import BaselinePageMixin, BaselineWorker
    from .deploy_page import DeployPageMixin
    from .unit_tests_page import UnitTestsPageMixin

    class CommandWorker(QThread):
        """Run explicit compiler or test-infrastructure configure/build stages."""

        output_ready = Signal(str)
        stage_ready = Signal(str)
        progress_ready = Signal(int, int, str)
        completed = Signal(object, bool, str)

        def __init__(
            self,
            run_id: str,
            commands: Sequence[tuple[str, list[str], Path]],
            metadata: dict[str, Any],
        ) -> None:
            super().__init__()
            self.run_id = run_id
            self.commands = tuple(commands)
            self.metadata = metadata
            self.controller = ProcessController(self._output)
            self.store: EvidenceStore | None = None

        def run(self) -> None:
            outcomes: list[CaseOutcome] = []
            try:
                self.store = EvidenceStore(self.run_id, self.metadata)
                for index, (label, command, cwd) in enumerate(self.commands, start=1):
                    if self.controller.cancelled.is_set():
                        outcomes.extend(
                            cancelled_infrastructure_outcomes(
                                self.commands, index, str(self.store.log_path)
                            )
                        )
                        break
                    self.stage_ready.emit(label)
                    self._output(f"$ {subprocess.list2cmdline(command)}\n")
                    started = time.monotonic()
                    code, captured = self.controller.run(command, cwd)
                    duration = time.monotonic() - started
                    command_cancelled = self.controller.cancelled.is_set()
                    outcomes.append(
                        infrastructure_command_outcome(
                            index=index,
                            label=label,
                            exit_code=code,
                            duration_seconds=duration,
                            cancelled=command_cancelled,
                            evidence=str(self.store.log_path),
                            captured_output=captured,
                        )
                    )
                    if command_cancelled:
                        outcomes.extend(
                            cancelled_infrastructure_outcomes(
                                self.commands, index + 1, str(self.store.log_path)
                            )
                        )
                        break
                    if code != 0:
                        outcomes.extend(
                            blocked_infrastructure_outcomes(
                                self.commands,
                                index + 1,
                                str(self.store.log_path),
                                label,
                            )
                        )
                        break
                cancelled = self.controller.cancelled.is_set()
                self.store.finish(outcomes, cancelled)
                self.completed.emit(outcomes, cancelled, str(self.store.root))
            except Exception as error:
                if self.store is not None:
                    self.store.close()
                self.completed.emit([], False, f"ERROR: {type(error).__name__}: {error}")

        def cancel(self) -> None:
            self.controller.cancel()

        def _output(self, line: str) -> None:
            if self.store is not None:
                self.store.log(line)
            self.output_ready.emit(line)
            match = NINJA_PROGRESS_PATTERN.match(line.rstrip("\r\n"))
            if match is not None:
                self.progress_ready.emit(
                    int(match.group(1)),
                    int(match.group(2)),
                    match.group(3),
                )

    class SuiteWorker(QThread):
        """Execute one immutable variant plan and publish live GUI events."""

        output_ready = Signal(str)
        case_ready = Signal(str, str, str, float, str)
        progress_ready = Signal(int, int)
        preparation_ready = Signal(str, int, int, str)
        completed = Signal(object, bool, str, object)

        def __init__(
            self,
            run_id: str,
            config: BuilderConfig,
            variants: Sequence[RunVariant],
            metadata: dict[str, Any],
        ) -> None:
            super().__init__()
            self.run_id = run_id
            self.config = config
            self.variants = tuple(variants)
            self.metadata = metadata
            self.engine: ExecutionEngine | None = None
            self.store: EvidenceStore | None = None

        def run(self) -> None:
            try:
                timeout_budgets = load_variant_timeout_budgets(self.variants)
                metadata = dict(self.metadata)
                metadata["variant_timeout_policy"] = {
                    "history_status": "PASS",
                    "history_key": ["case_id", "build_mode", "optimization", "profile"],
                    "rounding": "ceil-to-whole-second",
                    "fallback_seconds": 45 * 60,
                    "loaded_once": True,
                }
                self.store = EvidenceStore(self.run_id, metadata)
                self.engine = ExecutionEngine(
                    self.config,
                    self.store,
                    self.output_ready.emit,
                    self.case_ready.emit,
                    self.progress_ready.emit,
                    timeout_budgets,
                    self.preparation_ready.emit,
                )
                outcomes, cancelled = self.engine.run(self.variants)
                preparation = self.engine.preparation_failure
                self.store.finish(outcomes, cancelled, preparation)
                self.completed.emit(
                    outcomes, cancelled, str(self.store.root), preparation
                )
            except Exception as error:
                if self.store is not None:
                    self.store.close()
                self.completed.emit(
                    [], False, f"ERROR: {type(error).__name__}: {error}", None
                )

        def cancel(self) -> None:
            if self.engine is not None:
                self.engine.cancel()

        def cancel_current(self, key: str) -> bool:
            """Forward one row-scoped cancellation without stopping the worker."""
            return self.engine is not None and self.engine.cancel_current(key)

    class MainWindow(
        UnitTestsPageMixin,
        BaselinePageMixin,
        DeployPageMixin,
        QMainWindow,
    ):
        """Present compiler-suite construction and execution as separate workflows."""

        TABLE_HEADERS = (
            "Ola",
            "Componente",
            "Contrato",
            "Caso",
            "Build",
            "Opt",
            "Perfil",
            "Ejecutor",
            "Progreso",
            "Estado",
            "Acción",
            "Duración",
            "Detalle / evidencia",
        )
        CASE_COLUMN = 3
        PROGRESS_COLUMN = 8
        STATUS_COLUMN = 9
        ACTION_COLUMN = 10
        DURATION_COLUMN = 11
        DETAIL_COLUMN = 12
        HISTORY_HEADERS = (
            "Fecha y hora",
            "Estado",
            "Resultados",
            "Configuración",
            "Pruebas",
            "Tipo",
            "Corrida",
            "Origen / directorio",
        )
        HISTORY_PATH_COLUMN = 7

        def __init__(self) -> None:
            super().__init__()
            self.catalog = _smoke_catalog() if smoke_only else CanonicalCatalog.load()
            self._command_worker_type = CommandWorker
            self._suite_worker_type = SuiteWorker
            self.worker: CommandWorker | SuiteWorker | BaselineWorker | None = None
            self.last_failed_variants: tuple[RunVariant, ...] = ()
            self.last_preparation_variants: tuple[RunVariant, ...] = ()
            self.failure_outcomes_by_key: dict[str, CaseOutcome] = {}
            self.failure_variants_by_key: dict[str, RunVariant] = {}
            self.current_outcomes_by_key: dict[str, CaseOutcome] = {}
            self.disabled_failure_keys: set[str] = set()
            self.visible_variants: tuple[RunVariant, ...] = ()
            self.variant_by_key: dict[str, RunVariant] = {}
            self.status_item_by_key: dict[str, QTableWidgetItem] = {}
            self.live_status_by_key: dict[str, str] = {}
            self.live_status_counts: Counter[str] = Counter()
            self.variant_position_by_key: dict[str, int] = {}
            self.status_filter_visible_rows = 0
            self.unit_variants_by_key: dict[str, RunVariant] = {}
            self.unit_status_items: dict[str, QTableWidgetItem] = {}
            self.unit_last_failed_variants: tuple[RunVariant, ...] = ()
            self.path_edits: dict[str, QLineEdit] = {}
            self.active_operation = ""
            self.active_suite_surface = "external"
            self.active_release_operation = ""
            self.active_modes: tuple[str, ...] = ()
            self.infrastructure_ninja_total = 0
            self.history_run_pages: dict[str, QWidget] = {}
            self.pending_exact_action: tuple[str, bool] | None = None
            self.exact_action_suppressed_until = 0.0
            self.exact_action_timer = QTimer(self)
            self.exact_action_timer.setSingleShot(True)
            self.exact_action_timer.timeout.connect(self._run_pending_exact_action)
            self.setWindowTitle(APP_NAME)
            self.resize(1600, 960)
            self._build_ui()
            config = BuilderConfig() if smoke_only else BuilderConfig.load()
            self._load_config(config)
            self._refresh_scope_values()
            self._refresh_catalog_summary()
            if not smoke_only:
                self._preview_plan()
                self._refresh_history()

        def _build_ui(self) -> None:
            central = QWidget()
            outer = QVBoxLayout(central)
            self.summary_label = QLabel()
            self.summary_label.setWordWrap(True)
            outer.addWidget(self.summary_label)
            self.tabs = QTabWidget()
            self.tabs.addTab(self._build_infrastructure_tab(), "Construcción")
            self.tabs.addTab(self._build_execution_tab(), "Pruebas externas")
            self.unit_tab_index = self.tabs.addTab(
                self._build_unit_tests_tab(), "Pruebas unitarias"
            )
            self.results_tab_index = self.tabs.addTab(
                self._build_results_tab(), "Resultados"
            )
            self.history_top_tab_index = self.tabs.addTab(
                self._build_history_tab(), "Historial"
            )
            self.baseline_tab_index = self.tabs.addTab(
                self._build_baseline_tab(), "Baseline"
            )
            self.deploy_tab_index = self.tabs.addTab(
                self._build_deploy_tab(), "Deploy"
            )
            outer.addWidget(self.tabs, 1)
            self.setCentralWidget(central)
            self.statusBar().showMessage("Listo")

        def _build_infrastructure_tab(self) -> QWidget:
            page = QWidget()
            layout = QVBoxLayout(page)
            notice = QLabel(
                "Primero construye NSC con Clang/LLVM/LLD; después configura y construye "
                "la infraestructura nueva de pruebas con ese compilador. Las rutas conocidas "
                "se descubren automáticamente y los campos permiten overrides explícitos."
            )
            notice.setWordWrap(True)
            layout.addWidget(notice)
            paths = QGroupBox("Herramientas y rutas")
            form = QFormLayout(paths)
            self._add_path_row(form, "nsc_executable", "NSC seleccionado", False)
            self._add_path_row(form, "nslink_executable", "NSLink", False)
            self._add_path_row(form, "compiler_build_root", "Build de NSC", True)
            self._add_path_row(form, "install_prefix", "Prefijo de instalación", True)
            self._add_path_row(form, "build_root", "Árbol de pruebas", True)
            self._add_path_row(form, "compiler_llvm_root", "Raíz LLVM 23", True)
            self._add_path_row(form, "llvm_dir", "LLVM_DIR derivado", True)
            self.path_edits["llvm_dir"].setReadOnly(True)
            self._add_path_row(form, "archive_executable", "Archivador", False)
            self._add_path_row(form, "comparison_nsc", "NSC de comparación", False)
            self._add_path_row(form, "build_id_header", "Build-id header", False)
            self.jobs_spin = QSpinBox()
            self.jobs_spin.setRange(1, 256)
            form.addRow("Trabajos paralelos", self.jobs_spin)
            layout.addWidget(paths)
            compiler_group = QGroupBox("1. Construcción del compilador NSC")
            compiler_controls = QHBoxLayout(compiler_group)
            self.compiler_mode = QComboBox()
            self.compiler_mode.addItems(["Release", "Debug", "Todos"])
            self.compiler_linkage = QComboBox()
            self.compiler_linkage.addItems(["Static", "Dynamic"])
            self.compiler_optimization = QComboBox()
            self.compiler_optimization.addItems(["O0", "O1", "O2", "O3"])
            self.compiler_configure_button = QPushButton("Configurar NSC")
            self.compiler_build_button = QPushButton("Construir NSC")
            self.clean_compiler_button = QPushButton("Limpiar compilador")
            self.clean_compiler_button.setToolTip(
                "Elimina únicamente los árboles Debug y Release generados para NSC."
            )
            self.clean_test_tree_button = QPushButton("Limpiar árbol de pruebas")
            self.clean_test_tree_button.setToolTip(
                "Elimina únicamente los builds Debug y Release de la infraestructura "
                "de pruebas configurada."
            )
            self.infrastructure_cancel_button = QPushButton("Cancelar construcción")
            self.infrastructure_cancel_button.setEnabled(False)
            compiler_controls.addWidget(QLabel("Build"))
            compiler_controls.addWidget(self.compiler_mode)
            compiler_controls.addWidget(QLabel("Enlace host"))
            compiler_controls.addWidget(self.compiler_linkage)
            compiler_controls.addWidget(QLabel("Optimización host"))
            compiler_controls.addWidget(self.compiler_optimization)
            compiler_controls.addWidget(self.compiler_configure_button)
            compiler_controls.addWidget(self.compiler_build_button)
            compiler_controls.addWidget(self.clean_compiler_button)
            compiler_controls.addWidget(self.clean_test_tree_button)
            compiler_controls.addWidget(self.infrastructure_cancel_button)
            compiler_controls.addStretch(1)
            layout.addWidget(compiler_group)
            tests_group = QGroupBox("2. Infraestructura de pruebas")
            controls = QHBoxLayout(tests_group)
            self.infrastructure_mode = QComboBox()
            self.infrastructure_mode.addItems(["Debug", "Release", "Todos"])
            controls.addWidget(QLabel("Build"))
            controls.addWidget(self.infrastructure_mode)
            self.save_button = QPushButton("Guardar configuración")
            self.detect_button = QPushButton("Autodetectar rutas")
            self.configure_button = QPushButton("Configurar pruebas")
            self.build_button = QPushButton("Construir infraestructura")
            self.receipt_button = QPushButton("Validar compilador externo")
            controls.addWidget(self.save_button)
            controls.addWidget(self.detect_button)
            controls.addWidget(self.receipt_button)
            controls.addWidget(self.configure_button)
            controls.addWidget(self.build_button)
            controls.addStretch(1)
            layout.addWidget(tests_group)
            self.infrastructure_stage = QLabel("Sin operación activa")
            layout.addWidget(self.infrastructure_stage)
            self.infrastructure_progress = QProgressBar()
            self.infrastructure_progress.setRange(0, 1)
            self.infrastructure_progress.setValue(0)
            self.infrastructure_progress.setFormat("Sin pasos de Ninja")
            layout.addWidget(self.infrastructure_progress)
            self.infrastructure_progress_detail = QLabel("Sin actividad de construcción")
            self.infrastructure_progress_detail.setWordWrap(True)
            layout.addWidget(self.infrastructure_progress_detail)
            self.infrastructure_log = QTextEdit()
            self.infrastructure_log.setReadOnly(True)
            layout.addWidget(self.infrastructure_log, 1)
            self.save_button.clicked.connect(self._save_config)
            self.detect_button.clicked.connect(self._redetect_config)
            self.receipt_button.clicked.connect(self._validate_receipt)
            self.compiler_configure_button.clicked.connect(
                lambda: self._start_infrastructure("compiler-configure")
            )
            self.compiler_build_button.clicked.connect(
                lambda: self._start_infrastructure("compiler-build")
            )
            self.clean_compiler_button.clicked.connect(self._clean_compiler)
            self.clean_test_tree_button.clicked.connect(self._clean_test_tree)
            self.infrastructure_cancel_button.clicked.connect(self._cancel_active)
            self.configure_button.clicked.connect(
                lambda: self._start_infrastructure("tests-configure")
            )
            self.build_button.clicked.connect(
                lambda: self._start_infrastructure("tests-build")
            )
            return page

        def _build_execution_tab(self) -> QWidget:
            page = QWidget()
            layout = QVBoxLayout(page)
            filters = QGroupBox("Selección granular")
            grid = QGridLayout(filters)
            self.scope_combo = QComboBox()
            self.scope_combo.addItems(["Total", "Ola", "Componente", "Contrato", "Caso"])
            self.scope_value = QComboBox()
            self.scope_value.setEditable(True)
            self.profile_combo = QComboBox()
            self.profile_combo.addItems(["Todos", "PR", "NIGHTLY", "WEEKLY"])
            self.execution_mode = QComboBox()
            self.execution_mode.addItems(["Debug", "Release", "Todos"])
            self.optimization_combo = QComboBox()
            self.optimization_combo.addItems(["O0", "O1", "O2", "O3", "Todas"])
            grid.addWidget(QLabel("Alcance"), 0, 0)
            grid.addWidget(self.scope_combo, 0, 1)
            grid.addWidget(QLabel("Valor"), 0, 2)
            grid.addWidget(self.scope_value, 0, 3)
            grid.addWidget(QLabel("Perfil"), 0, 4)
            grid.addWidget(self.profile_combo, 0, 5)
            grid.addWidget(QLabel("Build"), 1, 0)
            grid.addWidget(self.execution_mode, 1, 1)
            grid.addWidget(QLabel("Optimización NativeSharp"), 1, 2)
            grid.addWidget(self.optimization_combo, 1, 3)
            self.preview_button = QPushButton("Previsualizar")
            self.run_button = QPushButton("Ejecutar pruebas")
            self.cancel_button = QPushButton("Cancelar corrida/árbol")
            self.retry_button = QPushButton("Reejecutar fallos")
            self.preparation_retry_button = QPushButton(
                "Reintentar preparación/corrida"
            )
            self.cancel_button.setEnabled(False)
            self.retry_button.setEnabled(False)
            self.preparation_retry_button.setEnabled(False)
            grid.addWidget(self.preview_button, 1, 4)
            grid.addWidget(self.run_button, 1, 5)
            grid.addWidget(self.cancel_button, 2, 4)
            grid.addWidget(self.retry_button, 2, 5)
            grid.addWidget(self.preparation_retry_button, 3, 5)
            layout.addWidget(filters)
            self.plan_label = QLabel()
            layout.addWidget(self.plan_label)
            status_filter_bar = QHBoxLayout()
            status_filter_bar.addWidget(QLabel("Mostrar"))
            self.status_filter_combo = QComboBox()
            for label, value in (
                ("Todos los estados", "ALL"),
                ("Fallos", "FAILURES"),
                ("En curso", "ACTIVE"),
                ("Pendientes / listos", "WAITING"),
                ("PASS", "PASS"),
                ("FAIL", "FAIL"),
                ("BLOCKED", "BLOCKED"),
                ("INFRASTRUCTURE_ERROR", "INFRASTRUCTURE_ERROR"),
                ("CANCELLED", "CANCELLED"),
                ("TIMEOUT", "TIMEOUT"),
                ("NOT_RUN / BLOQUEADO_POR", "NOT_RUN"),
                ("RUNNING", "RUNNING"),
                ("COMPILING", "COMPILING"),
                ("LISTO", "LISTO"),
                ("PENDIENTE", "PENDIENTE"),
                ("FILTRADO", "FILTRADO"),
            ):
                self.status_filter_combo.addItem(label, value)
            self.status_filter_summary = QLabel("Mostrando todas las filas")
            status_filter_bar.addWidget(self.status_filter_combo)
            status_filter_bar.addWidget(self.status_filter_summary)
            status_filter_bar.addStretch(1)
            layout.addLayout(status_filter_bar)
            self.clear_results_button = QPushButton(
                "Limpiar resultados para nueva corrida"
            )
            status_filter_bar.addWidget(self.clear_results_button)
            self.execution_activity = QLabel("Sin ejecución activa")
            self.execution_activity.setWordWrap(True)
            layout.addWidget(self.execution_activity)
            self.execution_counts = QLabel("Sin resultados")
            self.execution_counts.setWordWrap(True)
            layout.addWidget(self.execution_counts)
            self.progress = QProgressBar()
            self.progress.setRange(0, 1)
            self.progress.setValue(0)
            self.progress.setFormat("0/0 pruebas")
            layout.addWidget(self.progress)
            self.case_table = QTableWidget(0, len(self.TABLE_HEADERS))
            self.case_table.setHorizontalHeaderLabels(self.TABLE_HEADERS)
            self.case_table.setAlternatingRowColors(True)
            self.case_table.setSelectionBehavior(QTableWidget.SelectionBehavior.SelectRows)
            self.case_table.setSelectionMode(
                QAbstractItemView.SelectionMode.SingleSelection
            )
            self.case_table.setEditTriggers(QTableWidget.EditTrigger.NoEditTriggers)
            self.case_table.setContextMenuPolicy(
                Qt.ContextMenuPolicy.CustomContextMenu
            )
            case_header = self.case_table.horizontalHeader()
            case_header.setSectionResizeMode(QHeaderView.ResizeMode.Interactive)
            for column, width in enumerate(
                (55, 190, 170, 180, 75, 55, 75, 180, 100, 125, 125, 90)
            ):
                case_header.resizeSection(column, width)
            case_header.setStretchLastSection(True)
            layout.addWidget(self.case_table, 1)
            self.scope_combo.currentTextChanged.connect(self._refresh_scope_values)
            self.preview_button.clicked.connect(self._preview_plan)
            self.run_button.clicked.connect(self._start_suite)
            self.cancel_button.clicked.connect(self._cancel_active)
            self.retry_button.clicked.connect(self._retry_failures)
            self.preparation_retry_button.clicked.connect(
                self._retry_preparation
            )
            self.status_filter_combo.currentIndexChanged.connect(
                self._apply_status_filter
            )
            self.clear_results_button.clicked.connect(self._clear_current_results)
            self.case_table.cellClicked.connect(self._handle_case_table_click)
            self.case_table.cellDoubleClicked.connect(
                self._open_current_case_detail
            )
            self.case_table.customContextMenuRequested.connect(
                self._show_case_context_menu
            )
            return page

        def _build_results_tab(self) -> QWidget:
            page = QWidget()
            layout = QVBoxLayout(page)
            self.result_tabs = QTabWidget()
            self.full_log = QTextEdit()
            self.full_log.setReadOnly(True)
            self.error_report = QTextEdit()
            self.error_report.setReadOnly(True)
            failure_page = QWidget()
            failure_layout = QVBoxLayout(failure_page)
            self.failure_summary = QLabel("No hay fallos terminales registrados")
            failure_layout.addWidget(self.failure_summary)
            failure_controls = QHBoxLayout()
            self.failure_retry_button = QPushButton("Reejecutar fallos activos")
            self.failure_disable_button = QPushButton(
                "Desactivar seleccionados para reejecución"
            )
            self.failure_enable_button = QPushButton("Reactivar seleccionados")
            failure_controls.addWidget(self.failure_retry_button)
            failure_controls.addWidget(self.failure_disable_button)
            failure_controls.addWidget(self.failure_enable_button)
            failure_controls.addStretch(1)
            failure_layout.addLayout(failure_controls)
            self.failure_table = QTableWidget(0, len(self.TABLE_HEADERS))
            self.failure_table.setHorizontalHeaderLabels(self.TABLE_HEADERS)
            self.failure_table.setAlternatingRowColors(True)
            self.failure_table.setSelectionBehavior(
                QTableWidget.SelectionBehavior.SelectRows
            )
            self.failure_table.setSelectionMode(
                QAbstractItemView.SelectionMode.ExtendedSelection
            )
            self.failure_table.setEditTriggers(QTableWidget.EditTrigger.NoEditTriggers)
            self.failure_table.setContextMenuPolicy(
                Qt.ContextMenuPolicy.CustomContextMenu
            )
            self.failure_table.horizontalHeader().setSectionResizeMode(
                QHeaderView.ResizeMode.ResizeToContents
            )
            self.failure_table.horizontalHeader().setStretchLastSection(True)
            self.failure_table.cellClicked.connect(
                self._handle_failure_table_click
            )
            self.failure_table.cellDoubleClicked.connect(
                self._open_failure_case_detail
            )
            self.failure_table.customContextMenuRequested.connect(
                self._show_failure_context_menu
            )
            failure_layout.addWidget(self.failure_table, 1)
            self.result_tabs.addTab(self.full_log, "Log completo")
            self.result_tabs.addTab(self.error_report, "Errores condensados")
            self.failure_tab_index = self.result_tabs.addTab(
                failure_page, "Fallos exactos (0)"
            )
            self.failure_retry_button.clicked.connect(self._retry_failures)
            self.failure_disable_button.clicked.connect(
                lambda: self._set_selected_failures_enabled(False)
            )
            self.failure_enable_button.clicked.connect(
                lambda: self._set_selected_failures_enabled(True)
            )
            self._refresh_failure_controls()
            layout.addWidget(self.result_tabs)
            return page

        def _build_history_tab(self) -> QWidget:
            """Build the top-level historical workspace, separate from live results."""
            page = QWidget()
            layout = QVBoxLayout(page)
            self.history_workspace_tabs = QTabWidget()
            self.history_workspace_tabs.setObjectName("history_workspace_tabs")
            launcher_page = QWidget()
            history_layout = QVBoxLayout(launcher_page)
            self.history_table = QTableWidget(0, len(self.HISTORY_HEADERS))
            self.history_table.setHorizontalHeaderLabels(self.HISTORY_HEADERS)
            self.history_table.setSelectionBehavior(
                QTableWidget.SelectionBehavior.SelectRows
            )
            self.history_table.setEditTriggers(
                QTableWidget.EditTrigger.NoEditTriggers
            )
            self.history_table.horizontalHeader().setSectionResizeMode(
                QHeaderView.ResizeMode.ResizeToContents
            )
            self.history_table.horizontalHeader().setStretchLastSection(True)
            history_intro = QLabel(
                f"Corridas guardadas en {HISTORY_ROOT}. Selecciona una fila para ver "
                "qué se ejecutó, sus resultados y su log; abrirla crea un tab "
                "independiente y no modifica Pruebas."
            )
            history_intro.setWordWrap(True)
            history_layout.addWidget(history_intro)
            history_controls = QHBoxLayout()
            self.history_refresh_button = QPushButton("Actualizar lista")
            self.history_open_folder_button = QPushButton(
                "Abrir carpeta de evidencia"
            )
            self.history_load_button = QPushButton(
                "Abrir corrida completa en un tab"
            )
            history_controls.addWidget(self.history_refresh_button)
            history_controls.addWidget(self.history_open_folder_button)
            history_controls.addWidget(self.history_load_button)
            history_controls.addStretch(1)
            history_layout.addLayout(history_controls)
            history_layout.addWidget(self.history_table, 2)
            history_layout.addWidget(QLabel("Vista previa de la corrida seleccionada"))
            self.history_preview_tabs = QTabWidget()
            self.history_preview_summary = QPlainTextEdit()
            self.history_preview_summary.setReadOnly(True)
            self.history_preview_log = QPlainTextEdit()
            self.history_preview_log.setReadOnly(True)
            self.history_preview_report = QPlainTextEdit()
            self.history_preview_report.setReadOnly(True)
            self.history_preview_tabs.addTab(self.history_preview_summary, "Resumen")
            self.history_preview_tabs.addTab(self.history_preview_log, "Log")
            self.history_preview_tabs.addTab(
                self.history_preview_report, "Reporte guardado"
            )
            history_layout.addWidget(self.history_preview_tabs, 1)
            self.history_refresh_button.clicked.connect(self._refresh_history)
            self.history_open_folder_button.clicked.connect(self._open_history)
            self.history_load_button.clicked.connect(self._load_history_from_table)
            self.history_table.itemSelectionChanged.connect(
                self._preview_history_selection
            )
            self.history_table.cellDoubleClicked.connect(
                lambda _row, _column: self._load_history_from_table()
            )
            self.history_launcher_tab_index = self.history_workspace_tabs.addTab(
                launcher_page, "Corridas guardadas"
            )
            layout.addWidget(self.history_workspace_tabs, 1)
            return page

        def _add_path_row(
            self, form: QFormLayout, key: str, label: str, directory: bool
        ) -> None:
            container = QWidget()
            row = QHBoxLayout(container)
            row.setContentsMargins(0, 0, 0, 0)
            edit = QLineEdit()
            button = QPushButton("Examinar…")
            button.clicked.connect(lambda: self._browse_path(key, directory))
            row.addWidget(edit, 1)
            row.addWidget(button)
            self.path_edits[key] = edit
            form.addRow(label, container)

        def _browse_path(self, key: str, directory: bool) -> None:
            current = self.path_edits[key].text().strip() or str(SOURCE_ROOT)
            if directory:
                value = QFileDialog.getExistingDirectory(self, "Seleccionar directorio", current)
            else:
                value, _ = QFileDialog.getOpenFileName(self, "Seleccionar archivo", current)
            if value:
                self.path_edits[key].setText(value)

        def _load_config(self, config: BuilderConfig) -> None:
            for key, edit in self.path_edits.items():
                edit.setText(str(getattr(config, key)))
            self.jobs_spin.setValue(config.jobs)

        def _redetect_config(self) -> None:
            try:
                discovered = self._config_from_ui().with_discovery()
                self._load_config(discovered)
                tools = discover_build_tools(str(discovered.llvm_root_path()))
                lines = ["RUTAS AUTODETECTADAS", "=" * 72]
                lines.extend(f"{name}: {value}" for name, value in tools.items())
                lines.extend(
                    [
                        f"NSC: {discovered.nsc_executable}",
                        f"NSLink: {discovered.nslink_executable}",
                        f"Fuentes NSC: {SOURCE_ROOT}",
                        f"LLVM root de NSC: {discovered.compiler_llvm_root}",
                        f"Fuentes de pruebas: {SUITE_ROOT}",
                    ]
                )
                self.infrastructure_log.setPlainText("\n".join(lines))
            except Exception as error:
                self._show_error("No se pudieron autodetectar las rutas", error)

        def _config_from_ui(self) -> BuilderConfig:
            return BuilderConfig(
                nsc_executable=self.path_edits["nsc_executable"].text().strip(),
                nslink_executable=self.path_edits["nslink_executable"].text().strip(),
                compiler_build_root=self.path_edits["compiler_build_root"].text().strip(),
                install_prefix=self.path_edits["install_prefix"].text().strip(),
                build_root=self.path_edits["build_root"].text().strip(),
                compiler_llvm_root=self.path_edits["compiler_llvm_root"].text().strip(),
                llvm_dir=self.path_edits["llvm_dir"].text().strip(),
                archive_executable=self.path_edits["archive_executable"].text().strip(),
                comparison_nsc=self.path_edits["comparison_nsc"].text().strip(),
                build_id_header=self.path_edits["build_id_header"].text().strip(),
                jobs=self.jobs_spin.value(),
            )

        def _save_config(self) -> None:
            try:
                config = self._config_from_ui().with_discovery().validated(
                    require_tools=False
                )
                config.save()
                self.statusBar().showMessage(f"Configuración guardada en {CONFIG_PATH}", 8000)
            except Exception as error:
                self._show_error("Configuración inválida", error)

        def _validate_receipt(self) -> None:
            try:
                config = self._config_from_ui()
                receipt = compiler_receipt(config)
                config.save()
                lines = ["RECIBO DEL COMPILADOR EXTERNO", "=" * 72]
                for tool in receipt["tools"]:
                    lines.extend(
                        [
                            f"{tool['name']}: {tool['path']}",
                            f"  SHA-256: {tool['sha256']}",
                            f"  Tamaño: {tool['size']} bytes",
                        ]
                    )
                self.infrastructure_log.setPlainText("\n".join(lines))
            except Exception as error:
                self._show_error("No se pudo validar el recibo", error)

        def _clean_test_tree(self) -> None:
            """Remove only generated test builds after an exact-path confirmation."""
            if self.worker is not None:
                QMessageBox.warning(
                    self,
                    "Hay un proceso activo",
                    "No se puede limpiar el árbol de pruebas mientras Builder 3 "
                    "ejecuta o construye un proceso. Cancélalo y espera a que termine "
                    "antes de volver a intentarlo.",
                )
                self.statusBar().showMessage(
                    "Limpieza rechazada: todavía hay un proceso activo", 8000
                )
                return
            try:
                config = self._config_from_ui().with_discovery().validated(
                    require_tools=False
                )
                targets = test_tree_cleanup_targets(config)
            except Exception as error:
                self._show_error("No se puede limpiar el árbol de pruebas", error)
                return

            existing = tuple(path for path in targets if path.exists())
            path_lines = [
                f"• {path} ({'existe' if path.exists() else 'no existe'})"
                for path in targets
            ]
            if not existing:
                message = (
                    "No hay artefactos generados que limpiar en:\n\n"
                    + "\n".join(path_lines)
                )
                self.infrastructure_stage.setText("Árbol de pruebas ya limpio")
                self.infrastructure_progress_detail.setText(message)
                self.infrastructure_log.setPlainText(message)
                self.statusBar().showMessage("Árbol de pruebas ya limpio", 8000)
                self._preview_plan()
                self._refresh_history()
                QMessageBox.information(self, "Árbol de pruebas limpio", message)
                return

            prompt = (
                "Se eliminarán únicamente estos árboles generados de pruebas:\n\n"
                + "\n".join(path_lines)
                + "\n\nLa limpieza incluye binarios, objetos y archivos de "
                "configuración/build contenidos en esas rutas. No modifica fuentes, "
                "manifiestos, historial/evidencias, la instalación de NSC ni el build "
                "del compilador. ¿Continuar?"
            )
            answer = QMessageBox.warning(
                self,
                "Confirmar limpieza del árbol de pruebas",
                prompt,
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
                QMessageBox.StandardButton.No,
            )
            if answer != QMessageBox.StandardButton.Yes:
                self.statusBar().showMessage("Limpieza cancelada por el usuario", 5000)
                return
            if self.worker is not None:
                QMessageBox.warning(
                    self,
                    "Hay un proceso activo",
                    "La limpieza no comenzó porque se inició otro proceso.",
                )
                return

            self._set_busy(True)
            self.infrastructure_stage.setText("Limpiando árbol generado de pruebas")
            self.infrastructure_progress.setRange(0, 0)
            self.infrastructure_progress.setFormat("Limpieza en curso")
            self.infrastructure_progress_detail.setText("\n".join(path_lines))
            try:
                result = clean_test_build_trees(config)
            except Exception as error:
                self._show_error("Falló la limpieza del árbol de pruebas", error)
                return
            finally:
                self._set_busy(False)

            lines = [
                "LIMPIEZA DEL ÁRBOL DE PRUEBAS",
                "=" * 72,
                *(f"Ruta: {path}" for path in result.targets),
                f"Archivos eliminados: {result.removed_files:,}",
                f"Directorios eliminados: {result.removed_directories:,}",
                f"Rutas ya ausentes: {len(result.missing_targets):,}",
            ]
            if result.failures:
                lines.extend(("", "ERRORES:"))
                lines.extend(result.failures)
            self.infrastructure_log.setPlainText("\n".join(lines))
            self.infrastructure_progress.setRange(0, 1)
            self.infrastructure_progress.setValue(1)
            self._preview_plan()
            self._refresh_history()
            if result.failures:
                self.infrastructure_stage.setText("Limpieza incompleta")
                self.infrastructure_progress.setFormat("Limpieza incompleta")
                self.infrastructure_progress_detail.setText(
                    f"{len(result.failures):,} rutas no pudieron eliminarse; "
                    "revisa el registro."
                )
                QMessageBox.critical(
                    self,
                    "Limpieza incompleta",
                    f"Se encontraron {len(result.failures):,} errores. "
                    "No se tocó ninguna ruta fuera de los árboles confirmados.",
                )
            else:
                self.infrastructure_stage.setText("Árbol de pruebas limpio")
                self.infrastructure_progress.setFormat("Limpieza terminada")
                self.infrastructure_progress_detail.setText(
                    f"Eliminados {result.removed_files:,} archivos y "
                    f"{result.removed_directories:,} directorios."
                )
                self.statusBar().showMessage("Árbol de pruebas limpio", 8000)
                QMessageBox.information(
                    self,
                    "Árbol de pruebas limpio",
                    "La próxima construcción/configuración de pruebas partirá de cero.",
                )

        def _clean_compiler(self) -> None:
            """Remove only generated compiler builds after exact-path confirmation."""
            if self.worker is not None:
                QMessageBox.warning(
                    self,
                    "Hay un proceso activo",
                    "No se puede limpiar el compilador mientras Builder 3 ejecuta o "
                    "construye un proceso. Cancélalo y espera a que termine.",
                )
                self.statusBar().showMessage(
                    "Limpieza del compilador rechazada: hay un proceso activo", 8000
                )
                return
            try:
                config = self._config_from_ui().with_discovery().validated(
                    require_tools=False
                )
                targets = compiler_build_cleanup_targets(config)
            except Exception as error:
                self._show_error("No se puede limpiar el compilador", error)
                return

            path_lines = [
                f"• {path} ({'existe' if path.exists() else 'no existe'})"
                for path in targets
            ]
            existing = tuple(path for path in targets if path.exists())
            if not existing:
                invalidated = self._invalidate_compiler_build_state(config, targets)
                message = (
                    "Los árboles generados del compilador ya están ausentes:\n\n"
                    + "\n".join(path_lines)
                )
                if invalidated:
                    message += "\n\nEstado en memoria invalidado: " + ", ".join(invalidated)
                self.infrastructure_stage.setText("Compilador ya limpio")
                self.infrastructure_progress_detail.setText(message)
                self.infrastructure_log.setPlainText(message)
                self.statusBar().showMessage("Compilador ya limpio", 8000)
                self._preview_plan()
                self._refresh_history()
                QMessageBox.information(self, "Compilador limpio", message)
                return

            prompt = (
                "Se eliminarán completamente solo estos árboles generados del compilador:\n\n"
                + "\n".join(path_lines)
                + "\n\nNo se modificarán fuentes, instalación/prefix, configuración "
                "persistente, historial/evidencias, builds de pruebas, NSLink ni "
                "herramientas externas. La próxima construcción de NSC reconfigurará "
                "y recompilará desde cero. ¿Continuar?"
            )
            answer = QMessageBox.warning(
                self,
                "Confirmar limpieza del compilador",
                prompt,
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
                QMessageBox.StandardButton.No,
            )
            if answer != QMessageBox.StandardButton.Yes:
                self.statusBar().showMessage(
                    "Limpieza del compilador cancelada por el usuario", 5000
                )
                return
            if self.worker is not None:
                QMessageBox.warning(
                    self,
                    "Hay un proceso activo",
                    "La limpieza no comenzó porque se inició otro proceso.",
                )
                return

            self._set_busy(True)
            self.infrastructure_stage.setText("Limpiando compilador")
            self.infrastructure_progress.setRange(0, 0)
            self.infrastructure_progress.setFormat("Limpieza en curso")
            self.infrastructure_progress_detail.setText("\n".join(path_lines))
            try:
                result = clean_compiler_build_trees(config)
            except Exception as error:
                self._show_error("Falló la limpieza del compilador", error)
                return
            finally:
                self._set_busy(False)

            invalidated = self._invalidate_compiler_build_state(config, result.targets)
            lines = [
                "LIMPIEZA DEL COMPILADOR",
                "=" * 72,
                *(f"Ruta: {path}" for path in result.targets),
                f"Archivos eliminados: {result.removed_files:,}",
                f"Directorios eliminados: {result.removed_directories:,}",
                f"Rutas ya ausentes: {len(result.missing_targets):,}",
                "Estado/recibos en memoria invalidados: "
                + (", ".join(invalidated) if invalidated else "ninguno"),
            ]
            if result.failures:
                lines.extend(("", "ERRORES PARCIALES:"))
                lines.extend(result.failures)
            self.infrastructure_log.setPlainText("\n".join(lines))
            self.infrastructure_progress.setRange(0, 1)
            self.infrastructure_progress.setValue(1)
            self.infrastructure_ninja_total = 0
            self.active_operation = ""
            self.active_modes = ()
            self._preview_plan()
            self._refresh_history()
            if result.failures:
                self.infrastructure_stage.setText("Limpieza del compilador incompleta")
                self.infrastructure_progress.setFormat("Limpieza incompleta")
                self.infrastructure_progress_detail.setText(
                    f"{len(result.failures):,} rutas no pudieron eliminarse; revisa el registro."
                )
                QMessageBox.critical(
                    self,
                    "Limpieza del compilador incompleta",
                    f"Se encontraron {len(result.failures):,} errores parciales. "
                    "No se tocó ninguna ruta fuera de los árboles confirmados.",
                )
            else:
                self.infrastructure_stage.setText("Compilador limpio")
                self.infrastructure_progress.setFormat("Limpieza terminada")
                self.infrastructure_progress_detail.setText(
                    f"Eliminados {result.removed_files:,} archivos y "
                    f"{result.removed_directories:,} directorios."
                )
                self.statusBar().showMessage("Compilador limpio", 8000)
                QMessageBox.information(
                    self,
                    "Compilador limpio",
                    "La próxima construcción de NSC partirá de cero.",
                )

        def _invalidate_compiler_build_state(
            self, config: BuilderConfig, targets: Sequence[Path]
        ) -> tuple[str, ...]:
            """Clear only transient UI selections owned by deleted build trees."""
            invalidated = compiler_cleanup_invalidated_fields(config, targets)
            for field_name in invalidated:
                self.path_edits[field_name].clear()
            self.infrastructure_ninja_total = 0
            self.active_operation = ""
            self.active_modes = ()
            return invalidated

        def _start_infrastructure(self, operation: str) -> None:
            if self.worker is not None:
                return
            try:
                config = self._config_from_ui().with_discovery().validated(
                    require_tools=False
                )
                self._load_config(config)
                compiler_operation = operation.startswith("compiler-")
                tools = (
                    discover_compiler_build_tools(str(config.llvm_root_path()))
                    if compiler_operation
                    else discover_build_tools(str(config.llvm_root_path()))
                )
                selected = (
                    self.compiler_mode.currentText()
                    if compiler_operation
                    else self.infrastructure_mode.currentText()
                )
                modes = ("Debug", "Release") if selected == "Todos" else (selected,)
                if compiler_operation:
                    nslink_commands = compiler_nslink_commands(config, modes)
                    configure_commands_for_compiler = compiler_configure_commands(
                        config,
                        modes,
                        self.compiler_linkage.currentText(),
                        self.compiler_optimization.currentText(),
                        use_sibling_nslink=True,
                    )
                    if operation == "compiler-configure":
                        commands = nslink_commands + configure_commands_for_compiler
                    else:
                        commands = (
                            nslink_commands
                            + configure_commands_for_compiler
                            + compiler_build_commands(config, modes)
                            + compiler_post_build_sibling_commands(config, modes)
                        )
                    receipt: dict[str, Any] = {
                        "captured_at": utc_now(),
                        "sibling_sources": {
                            "nslink": str(NSLINK_SOURCE_ROOT.resolve()),
                            "nsasm": str(NSASM_SOURCE_ROOT.resolve()),
                            "nsdbg": str(NSDBG_SOURCE_ROOT.resolve()),
                        },
                        "build_tools": tools,
                    }
                else:
                    config.validated(require_tools=True)
                    if operation == "tests-configure":
                        commands = configure_commands(config, modes)
                    else:
                        commands = configure_commands(config, modes) + build_commands(config, modes)
                    receipt = compiler_receipt(config)
                config.save()
                run_id = make_run_id()
                metadata = {
                    "schema": 1,
                    "kind": operation,
                    "run_id": run_id,
                    "revision": current_revision(),
                    "commands": [
                        {"label": label, "command": command, "cwd": str(cwd)}
                        for label, command, cwd in commands
                    ],
                    "variant_count": len(commands),
                    "tool_receipt": receipt,
                }
                self.infrastructure_log.clear()
                self.infrastructure_progress.setRange(0, 1)
                self.infrastructure_progress.setValue(0)
                self.infrastructure_progress.setFormat("Esperando pasos de Ninja")
                self.infrastructure_progress_detail.setText("Esperando salida completa del proceso")
                self.infrastructure_ninja_total = 0
                self.active_operation = operation
                self.active_modes = modes
                self._set_busy(True)
                worker = CommandWorker(run_id, commands, metadata)
                self.worker = worker
                worker.output_ready.connect(self._append_infrastructure_output)
                worker.stage_ready.connect(self._set_infrastructure_stage)
                worker.progress_ready.connect(self._update_infrastructure_progress)
                worker.completed.connect(self._infrastructure_finished)
                worker.start()
            except Exception as error:
                self._show_error("No se pudo iniciar la infraestructura", error)

        def _append_infrastructure_output(self, text: str) -> None:
            cursor = self.infrastructure_log.textCursor()
            cursor.movePosition(cursor.MoveOperation.End)
            cursor.insertText(text)
            self.infrastructure_log.setTextCursor(cursor)
            self.infrastructure_log.ensureCursorVisible()

        def _set_infrastructure_stage(self, stage: str) -> None:
            """Expose stage changes without discarding the unabridged console."""
            self.infrastructure_stage.setText(stage)
            self.infrastructure_progress.setRange(0, 1)
            self.infrastructure_progress.setValue(0)
            self.infrastructure_progress.setFormat("Esperando pasos de Ninja")
            self.infrastructure_progress_detail.setText("Esperando salida completa del proceso")
            self.infrastructure_ninja_total = 0

        def _update_infrastructure_progress(
            self,
            current: int,
            total: int,
            detail: str,
        ) -> None:
            """Render Ninja's own progress while retaining its original log line."""
            self.infrastructure_progress.setRange(0, max(total, 1))
            self.infrastructure_progress.setValue(min(current, total))
            self.infrastructure_progress.setFormat("%v/%m pasos de Ninja (%p%)")
            self.infrastructure_progress_detail.setText(detail or "Paso Ninja sin descripción")
            self.infrastructure_ninja_total = total

        def _infrastructure_finished(
            self, outcomes: object, cancelled: bool, evidence: str
        ) -> None:
            typed = list(outcomes) if isinstance(outcomes, list) else []
            failed = evidence.startswith("ERROR:") or any(
                item.status in TERMINAL_FAILURES for item in typed
            )
            preferred = ""
            compiler_products: tuple[Path, ...] = ()
            if not failed and not cancelled and self.active_operation == "compiler-build":
                preferred = "Release" if "Release" in self.active_modes else self.active_modes[0]
                config = self._config_from_ui()
                compiler_products = (
                    compiler_output(config, preferred),
                    compiler_sibling_output(config, preferred, "nslink"),
                    compiler_sibling_output(config, preferred, "nsasm"),
                    compiler_sibling_output(config, preferred, "nsdbg"),
                )
                missing = tuple(path for path in compiler_products if not path.is_file())
                if missing:
                    failed = True
                    evidence = "ERROR: faltan productos Win32: " + ", ".join(
                        str(path) for path in missing
                    )
            if evidence.startswith("ERROR:"):
                message = evidence
            elif cancelled:
                message = f"Operación cancelada. Evidencia: {evidence}"
            elif failed:
                message = f"Operación terminada con errores. Evidencia: {evidence}"
            else:
                message = f"Operación terminada correctamente. Evidencia: {evidence}"
                total = self.infrastructure_ninja_total
                self.infrastructure_progress.setRange(0, max(total, 1))
                self.infrastructure_progress.setValue(max(total, 1))
                if total:
                    self.infrastructure_progress.setFormat(
                        "%v/%m pasos de Ninja — TERMINADO (100%)"
                    )
                else:
                    self.infrastructure_progress.setFormat("TERMINADO (100%)")
                self.infrastructure_progress_detail.setText(
                    "TERMINADO: la operación concluyó correctamente."
                )
                if self.active_operation == "compiler-build":
                    config = self._config_from_ui()
                    compiler, nslink, nsasm, nsdbg = compiler_products
                    self.path_edits["nsc_executable"].setText(str(compiler))
                    self.path_edits["nslink_executable"].setText(str(nslink))
                    self._config_from_ui().save()
                    message += (
                        f" Toolchain Win32 seleccionado: {compiler}, {nslink}, "
                        f"{nsasm}, {nsdbg}"
                    )
            self.infrastructure_stage.setText(message)
            self._render_error_report(typed, cancelled, evidence)
            if failed:
                self.tabs.setCurrentIndex(self.results_tab_index)
                self.result_tabs.setCurrentIndex(1)
            self.active_operation = ""
            self.worker = None
            self._set_busy(False)
            self._refresh_history()

        def _refresh_catalog_summary(self) -> None:
            summary = self.catalog.summary()
            dispositions = ", ".join(
                f"{name}: {count}" for name, count in summary["dispositions"].items()
            )
            self.summary_label.setText(
                f"Fase 2 · {summary['cases']:,} casos · {summary['contracts']:,} contratos · "
                f"{summary['components']} componentes · {summary['executable']:,} ejecutables · "
                f"{dispositions}"
            )

        def _refresh_scope_values(self) -> None:
            scope = self.scope_combo.currentText()
            prior = self.scope_value.currentText()
            values: Sequence[str]
            if scope == "Ola":
                values = tuple(str(value) for value in self.catalog.waves)
            elif scope == "Componente":
                values = self.catalog.components
            elif scope == "Contrato":
                values = self.catalog.contracts
            elif scope == "Caso":
                values = tuple(case.case_id for case in self.catalog.cases)
            else:
                values = ()
            self.scope_value.blockSignals(True)
            self.scope_value.clear()
            self.scope_value.addItems(values)
            if prior in values:
                self.scope_value.setCurrentText(prior)
            self.scope_value.setEnabled(bool(values))
            self.scope_value.blockSignals(False)

        def _selected_cases(self) -> tuple[CaseRecord, ...]:
            scope = self.scope_combo.currentText()
            value = self.scope_value.currentText().strip()
            if scope != "Total" and not value:
                raise ValueError(f"selecciona un valor para el alcance {scope}")
            return self.catalog.select(scope, value, self.profile_combo.currentText())

        def _preview_plan(self) -> None:
            try:
                cases = self._selected_cases()
                variants = expand_variants(
                    cases,
                    self.execution_mode.currentText(),
                    self.optimization_combo.currentText(),
                )
                self.visible_variants = variants
                self._populate_plan(cases, variants)
            except Exception as error:
                self._show_error("No se pudo preparar la selección", error)

        def _populate_plan(
            self,
            cases: Sequence[CaseRecord],
            variants: Sequence[RunVariant],
            outcomes: Sequence[CaseOutcome] = (),
        ) -> None:
            executable_ids = {variant.case.case_id for variant in variants}
            outcome_by_key = {outcome.key: outcome for outcome in outcomes}
            pending = [case for case in cases if not case.executable]
            filtered = [
                case for case in cases if case.executable and case.case_id not in executable_ids
            ]
            rows: list[tuple[CaseRecord, str, str, str, str, float, str]] = []
            for variant in variants:
                outcome = outcome_by_key.get(variant.key)
                detail = ""
                duration = 0.0
                status = "LISTO"
                if outcome is not None:
                    status = outcome.status
                    duration = outcome.duration_seconds
                    detail = outcome.detail
                    if outcome.evidence:
                        detail = (
                            f"{detail} | {outcome.evidence}"
                            if detail
                            else outcome.evidence
                        )
                rows.append(
                    (
                    variant.case,
                    variant.build_mode,
                    variant.optimization,
                    variant.key,
                    status,
                    duration,
                    detail,
                    )
                )
            rows.extend(
                (
                    case,
                    "NA",
                    "NA",
                    f"{case.case_id}|NA|NA",
                    "PENDIENTE",
                    0.0,
                    case.disposition,
                )
                for case in pending
            )
            rows.extend(
                (
                    case,
                    "NA",
                    "NA",
                    f"{case.case_id}|FILTERED|FILTERED",
                    "FILTRADO",
                    0.0,
                    "la combinación Build/Opt elegida no está soportada por el caso",
                )
                for case in filtered
            )
            self.case_table.setSortingEnabled(False)
            self.case_table.setUpdatesEnabled(False)
            self.case_table.clearContents()
            self.case_table.setRowCount(len(rows))
            self.status_item_by_key.clear()
            self.variant_by_key = {variant.key: variant for variant in variants}
            self.live_status_by_key = {
                key: status
                for _case, _mode, _opt, key, status, _duration, _detail in rows
            }
            self.live_status_counts = Counter(self.live_status_by_key.values())
            self.variant_position_by_key = {
                variant.key: index
                for index, variant in enumerate(variants, start=1)
            }
            for row_index, (
                case,
                mode,
                opt,
                key,
                status,
                duration,
                detail,
            ) in enumerate(rows):
                values = (
                    str(case.wave),
                    case.component,
                    case.contract_id,
                    case.case_id,
                    mode,
                    opt,
                    case.profile,
                    case.executor,
                    row_progress_text(status),
                    status,
                    row_action_text(status, key in self.variant_by_key),
                    f"{duration:.1f} s" if duration else "",
                    detail,
                )
                for column, value in enumerate(values):
                    item = QTableWidgetItem(value)
                    if column == self.CASE_COLUMN:
                        item.setData(Qt.ItemDataRole.UserRole, key)
                        item.setToolTip(
                            "Doble clic para abrir el escenario, el oráculo y la salida completa"
                        )
                    if column == self.STATUS_COLUMN:
                        self._color_status(item, status)
                        self.status_item_by_key[key] = item
                    if column == self.ACTION_COLUMN and value:
                        item.setForeground(QColor("#70a7ff"))
                        item.setToolTip("Ejecutar solamente esta variante exacta")
                    self.case_table.setItem(row_index, column, item)
            self.case_table.setUpdatesEnabled(True)
            self.case_table.setSortingEnabled(True)
            self.plan_label.setText(
                f"Selección: {len(cases):,} casos canónicos · {len(variants):,} variantes "
                f"ejecutables · {len(pending):,} pendientes visibles · {len(filtered):,} filtradas"
            )
            self.progress.setRange(0, max(1, len(variants)))
            self.progress.setValue(0)
            self.progress.setFormat(f"0/{len(variants):,} pruebas (0%)")
            self.execution_activity.setText("Preparada; ninguna prueba ha iniciado")
            self._refresh_live_counts()
            self._apply_status_filter()

        def _current_status_filter(self) -> str:
            value = self.status_filter_combo.currentData()
            return value if isinstance(value, str) and value else "ALL"

        def _render_status_filter_summary(self) -> None:
            total = self.case_table.rowCount()
            self.status_filter_summary.setText(
                f"Mostrando {self.status_filter_visible_rows:,} de {total:,} filas"
            )

        def _apply_status_filter(self, _index: int | None = None) -> None:
            """Hide rows by status while preserving the immutable execution plan."""
            filter_name = self._current_status_filter()
            visible = 0
            self.case_table.setUpdatesEnabled(False)
            try:
                for row in range(self.case_table.rowCount()):
                    item = self.case_table.item(row, self.STATUS_COLUMN)
                    status = item.text() if item is not None else ""
                    matches = status_filter_matches(filter_name, status)
                    self.case_table.setRowHidden(row, not matches)
                    visible += int(matches)
            finally:
                self.case_table.setUpdatesEnabled(True)
            self.status_filter_visible_rows = visible
            self._render_status_filter_summary()

        def _apply_status_filter_to_row(self, row: int, status: str) -> None:
            """Refresh one changing row without rescanning the full test table."""
            was_visible = not self.case_table.isRowHidden(row)
            is_visible = status_filter_matches(self._current_status_filter(), status)
            self.case_table.setRowHidden(row, not is_visible)
            if was_visible != is_visible:
                self.status_filter_visible_rows += 1 if is_visible else -1
                self._render_status_filter_summary()

        def _start_suite(self) -> None:
            if self.worker is not None:
                return
            try:
                self.catalog = CanonicalCatalog.load()
                config = self._config_from_ui().with_discovery().validated(
                    require_tools=False
                )
                self._load_config(config)
                config.save()
                cases = self._selected_cases()
                variants = expand_variants(
                    cases,
                    self.execution_mode.currentText(),
                    self.optimization_combo.currentText(),
                )
                if not variants:
                    raise ValueError("la selección no contiene variantes ejecutables")
                self._populate_plan(cases, variants)
                self._run_variants(config, variants, "selection")
            except Exception as error:
                self._show_error("No se pudo iniciar la ejecución", error)

        def _case_key_at(self, table: QTableWidget, row: int) -> str | None:
            """Read the exact immutable variant key carried by one visible row."""
            item = table.item(row, self.CASE_COLUMN)
            if item is None:
                return None
            key = item.data(Qt.ItemDataRole.UserRole)
            return key if isinstance(key, str) and key else None

        def _start_exact_variant(self, variant: RunVariant) -> None:
            """Run one exact case/build/optimization variant without widening scope."""
            if self.worker is not None:
                return
            try:
                config = self._config_from_ui().with_discovery().validated(
                    require_tools=False
                )
                self._load_config(config)
                config.save()
                self._update_case(
                    variant.key,
                    "LISTO",
                    "ejecución individual preparada",
                    0.0,
                    "",
                )
                self._run_variants(config, (variant,), "single-exact-variant")
            except Exception as error:
                self._show_error("No se pudo ejecutar la prueba seleccionada", error)

        def _start_exact_key(self, key: str, *, failed: bool = False) -> None:
            """Resolve and start one exact key from the main or failure projection."""
            variants = self.failure_variants_by_key if failed else self.variant_by_key
            variant = variants.get(key)
            if variant is not None:
                self._start_exact_variant(variant)

        def _case_at(self, table: QTableWidget, row: int) -> CaseRecord | None:
            """Resolve the canonical case represented by one table row."""
            item = table.item(row, self.CASE_COLUMN)
            if item is None:
                return None
            return self.catalog.by_id.get(item.text())

        @staticmethod
        def _resolved_evidence_path(
            outcome: CaseOutcome | None, run_root: Path | None
        ) -> Path | None:
            """Resolve one evidence location without requiring it to still exist."""
            if outcome is None or not outcome.evidence:
                return None
            path = Path(outcome.evidence)
            if not path.is_absolute() and run_root is not None:
                path = run_root / path
            return path

        def _open_case_detail_tab(
            self,
            tab_widget: QTabWidget,
            case: CaseRecord,
            variant: RunVariant | None,
            outcome: CaseOutcome | None,
            run_root: Path | None = None,
        ) -> None:
            """Open a read-only case diagnosis without executing or rewriting it."""
            identity = variant.key if variant is not None else case.case_id
            detail_key = f"case-detail:{identity}"
            for index in range(tab_widget.count() - 1, -1, -1):
                existing = tab_widget.widget(index)
                if existing.property("caseDetailKey") == detail_key:
                    tab_widget.removeTab(index)
                    existing.deleteLater()

            page = QWidget()
            page.setProperty("caseDetailKey", detail_key)
            layout = QVBoxLayout(page)
            heading = QLabel(
                f"{case.case_id} · {case.component} · {case.contract_id} · "
                f"{outcome.status if outcome is not None else 'SIN RESULTADO'}"
            )
            heading.setWordWrap(True)
            layout.addWidget(heading)

            actions = QHBoxLayout()
            evidence_path = self._resolved_evidence_path(outcome, run_root)
            open_evidence = QPushButton("Abrir evidencia")
            open_evidence.setEnabled(
                evidence_path is not None and evidence_path.exists()
            )
            close_detail = QPushButton("Cerrar detalle")
            actions.addWidget(open_evidence)
            actions.addStretch(1)
            actions.addWidget(close_detail)
            layout.addLayout(actions)

            sections = QTabWidget()

            def add_text_section(title: str, contents: str) -> None:
                view = QPlainTextEdit()
                view.setReadOnly(True)
                view.setPlainText(contents)
                sections.addTab(view, title)

            add_text_section(
                "Resumen", case_diagnostic_summary(case, variant, outcome)
            )
            add_text_section(
                "Entradas y oráculo",
                json.dumps(
                    case_contract_payload(case),
                    ensure_ascii=False,
                    indent=2,
                    sort_keys=True,
                ),
            )
            add_text_section(
                "Salida completa", case_evidence_output(outcome, run_root)
            )
            add_text_section(
                "Manifiesto",
                json.dumps(
                    case.manifest,
                    ensure_ascii=False,
                    indent=2,
                    sort_keys=True,
                )
                if case.manifest is not None
                else "Este caso no posee todavía un manifiesto ejecutable.\n",
            )
            layout.addWidget(sections, 1)
            index = tab_widget.addTab(page, f"{case.case_id} · detalle")
            tab_widget.setCurrentIndex(index)

            def reveal_evidence() -> None:
                if evidence_path is None:
                    return
                target = (
                    evidence_path if evidence_path.is_dir() else evidence_path.parent
                )
                QDesktopServices.openUrl(QUrl.fromLocalFile(str(target)))

            def close_page() -> None:
                page_index = tab_widget.indexOf(page)
                if page_index >= 0:
                    tab_widget.removeTab(page_index)
                page.deleteLater()

            open_evidence.clicked.connect(reveal_evidence)
            close_detail.clicked.connect(close_page)

        def _cancel_pending_exact_action(self) -> None:
            """Prevent a double click from also triggering the single-click action."""
            self.exact_action_timer.stop()
            self.pending_exact_action = None
            self.exact_action_suppressed_until = time.monotonic() + 0.5

        def _run_pending_exact_action(self) -> None:
            pending = self.pending_exact_action
            self.pending_exact_action = None
            if pending is None or self.worker is not None:
                return
            key, failed = pending
            self._start_exact_key(key, failed=failed)

        def _queue_exact_action(
            self, table: QTableWidget, row: int, column: int, *, failed: bool
        ) -> None:
            if (
                column != self.ACTION_COLUMN
                or self.worker is not None
                or time.monotonic() < self.exact_action_suppressed_until
            ):
                return
            key = self._case_key_at(table, row)
            variants = (
                self.failure_variants_by_key if failed else self.variant_by_key
            )
            if key is None or key not in variants:
                return
            self.pending_exact_action = (key, failed)
            self.exact_action_timer.start(QApplication.doubleClickInterval() + 50)

        def _open_current_case_detail(self, row: int, _column: int) -> None:
            self._cancel_pending_exact_action()
            case = self._case_at(self.case_table, row)
            key = self._case_key_at(self.case_table, row)
            if case is None or key is None:
                return
            self._open_case_detail_tab(
                self.result_tabs,
                case,
                self.variant_by_key.get(key),
                self.current_outcomes_by_key.get(key),
            )
            self.tabs.setCurrentIndex(self.results_tab_index)

        def _open_failure_case_detail(self, row: int, _column: int) -> None:
            self._cancel_pending_exact_action()
            case = self._case_at(self.failure_table, row)
            key = self._case_key_at(self.failure_table, row)
            if case is None or key is None:
                return
            self._open_case_detail_tab(
                self.result_tabs,
                case,
                self.failure_variants_by_key.get(key),
                self.failure_outcomes_by_key.get(key),
            )
            self.tabs.setCurrentIndex(self.results_tab_index)

        def _handle_case_table_click(self, row: int, column: int) -> None:
            if column == self.ACTION_COLUMN:
                key = self._case_key_at(self.case_table, row)
                if (
                    key is not None
                    and self.live_status_by_key.get(key) == "RUNNING"
                    and isinstance(self.worker, SuiteWorker)
                ):
                    self._cancel_current_variant(key)
                    return
            self._queue_exact_action(
                self.case_table, row, column, failed=False
            )

        def _cancel_current_variant(self, key: str) -> None:
            """Request cancellation only for the exact active RUNNING row."""
            worker = self.worker
            if isinstance(worker, SuiteWorker) and worker.cancel_current(key):
                self.statusBar().showMessage(
                    f"Cancelación solicitada únicamente para {key}; la corrida continuará"
                )

        def _show_case_context_menu(self, position: Any) -> None:
            row = self.case_table.rowAt(position.y())
            if row < 0:
                return
            key = self._case_key_at(self.case_table, row)
            variant = self.variant_by_key.get(key or "")
            if variant is None:
                return
            self.case_table.selectRow(row)
            status = self.live_status_by_key.get(variant.key, "LISTO")
            menu = QMenu(self.case_table)
            if status == "RUNNING" and isinstance(self.worker, SuiteWorker):
                action = menu.addAction("Cancelar prueba")
                action.triggered.connect(
                    lambda: self._cancel_current_variant(variant.key)
                )
            else:
                label = (
                    "Volver a correr solamente esta prueba"
                    if status in RESULT_STATUSES
                    else "Ejecutar solamente esta prueba"
                )
                action = menu.addAction(label)
                action.setEnabled(self.worker is None)
                action.triggered.connect(lambda: self._start_exact_key(variant.key))
            menu.exec(self.case_table.viewport().mapToGlobal(position))

        def _handle_failure_table_click(self, row: int, column: int) -> None:
            self._queue_exact_action(
                self.failure_table, row, column, failed=True
            )

        def _show_failure_context_menu(self, position: Any) -> None:
            row = self.failure_table.rowAt(position.y())
            if row < 0:
                return
            key = self._case_key_at(self.failure_table, row)
            if key is None or key not in self.failure_variants_by_key:
                return
            self.failure_table.selectRow(row)
            menu = QMenu(self.failure_table)
            action = menu.addAction("Volver a correr solamente esta prueba")
            action.setEnabled(self.worker is None)
            action.triggered.connect(
                lambda: self._start_exact_key(key, failed=True)
            )
            menu.exec(self.failure_table.viewport().mapToGlobal(position))

        def _run_variants(
            self,
            config: BuilderConfig,
            variants: Sequence[RunVariant],
            origin: str,
        ) -> None:
            self.active_suite_surface = "external"
            self.catalog = CanonicalCatalog.load()
            variants = refresh_run_variants(self.catalog, variants)
            config = config.with_discovery().validated(require_tools=False)
            self._load_config(config)
            config.save()
            self.visible_variants = tuple(variants)
            self.current_outcomes_by_key.clear()
            self.variant_position_by_key = {
                variant.key: index
                for index, variant in enumerate(variants, start=1)
            }
            run_id = make_run_id()
            metadata = {
                "schema": 1,
                "kind": "phase-2-suite",
                "origin": origin,
                "run_id": run_id,
                "revision": current_revision(),
                "variant_count": len(variants),
                "case_count": len({variant.case.case_id for variant in variants}),
                "selection": {
                    "scope": self.scope_combo.currentText(),
                    "value": self.scope_value.currentText(),
                    "profile": self.profile_combo.currentText(),
                    "build_mode": self.execution_mode.currentText(),
                    "optimization": self.optimization_combo.currentText(),
                },
            }
            self.full_log.clear()
            self.error_report.clear()
            self.tabs.setCurrentIndex(1)
            self.case_table.setSortingEnabled(False)
            self.execution_activity.setText("Preparar pruebas — planificando targets")
            self._set_busy(True)
            worker = SuiteWorker(run_id, config, variants, metadata)
            self.worker = worker
            worker.output_ready.connect(self._append_full_output)
            worker.case_ready.connect(self._update_case)
            worker.progress_ready.connect(self._update_progress)
            worker.preparation_ready.connect(self._update_preparation)
            worker.completed.connect(self._suite_finished)
            worker.finished.connect(self._release_finished_suite_worker)
            worker.start()

        def _retry_failures(self) -> None:
            if self.worker is not None or not self.last_failed_variants:
                return
            try:
                config = self._config_from_ui().validated(require_tools=False)
                variants = active_retry_variants(
                    self.last_failed_variants, self.disabled_failure_keys
                )
                if not variants:
                    raise ValueError(
                        "todos los fallos están desactivados para esta reejecución"
                    )
                self._populate_plan(
                    tuple(variant.case for variant in variants),
                    variants,
                )
                self._run_variants(config, variants, "rerun-failures")
            except Exception as error:
                self._show_error("No se pudieron reejecutar los fallos", error)

        def _retry_preparation(self) -> None:
            """Retry the complete immutable selection after preparation failed."""
            if self.worker is not None or not self.last_preparation_variants:
                return
            try:
                config = self._config_from_ui().validated(require_tools=False)
                config.save()
                variants = self.last_preparation_variants
                self._populate_plan(
                    tuple(variant.case for variant in variants), variants
                )
                self._run_variants(config, variants, "retry-preparation")
            except Exception as error:
                self._show_error("No se pudo reintentar la preparación", error)

        def _set_selected_failures_enabled(self, enabled: bool) -> None:
            """Toggle only selected exact variants in the directed retry plan."""
            rows = {
                index.row()
                for index in self.failure_table.selectionModel().selectedRows()
            }
            for row in rows:
                case_item = self.failure_table.item(row, 3)
                if case_item is None:
                    continue
                key = case_item.data(Qt.ItemDataRole.UserRole)
                if not isinstance(key, str) or key not in self.failure_variants_by_key:
                    continue
                if enabled:
                    self.disabled_failure_keys.discard(key)
                else:
                    self.disabled_failure_keys.add(key)
            self._populate_failure_table()

        def _populate_failure_table(self) -> None:
            """Render one row per exact terminal variant retained for retry."""
            variants = tuple(
                self.failure_variants_by_key[key]
                for key in sorted(self.failure_variants_by_key)
            )
            self.failure_table.setSortingEnabled(False)
            self.failure_table.clearContents()
            self.failure_table.setRowCount(len(variants))
            for row, variant in enumerate(variants):
                outcome = self.failure_outcomes_by_key[variant.key]
                disabled = variant.key in self.disabled_failure_keys
                status = "DESACTIVADA" if disabled else outcome.status
                detail = outcome.detail
                if outcome.evidence:
                    detail = (
                        f"{detail} | {outcome.evidence}"
                        if detail
                        else outcome.evidence
                    )
                values = (
                    str(variant.case.wave),
                    variant.case.component,
                    variant.case.contract_id,
                    variant.case.case_id,
                    variant.build_mode,
                    variant.optimization,
                    variant.case.profile,
                    variant.case.executor,
                    row_progress_text(outcome.status),
                    status,
                    "Volver a correr",
                    f"{outcome.duration_seconds:.3f} s",
                    detail,
                )
                for column, value in enumerate(values):
                    item = QTableWidgetItem(value)
                    if column == self.CASE_COLUMN:
                        item.setData(Qt.ItemDataRole.UserRole, variant.key)
                        item.setToolTip(
                            "Doble clic para abrir el escenario, el oráculo y la salida completa"
                        )
                    if column == self.STATUS_COLUMN:
                        self._color_status(item, status)
                    if column == self.ACTION_COLUMN:
                        item.setForeground(QColor("#70a7ff"))
                        item.setToolTip("Volver a correr solamente esta variante exacta")
                    self.failure_table.setItem(row, column, item)
            self.failure_table.setSortingEnabled(True)
            active = len(variants) - len(self.disabled_failure_keys)
            disabled = len(self.disabled_failure_keys)
            self.failure_summary.setText(
                f"Fallos exactos: {len(variants):,} · activos para reejecución: "
                f"{active:,} · desactivados temporalmente: {disabled:,}"
            )
            self.result_tabs.setTabText(
                self.failure_tab_index, f"Fallos exactos ({len(variants):,})"
            )
            self._refresh_failure_controls()

        def _refresh_failure_controls(self) -> None:
            """Keep retry controls consistent with active and disabled failures."""
            idle = self.worker is None
            active = len(self.failure_variants_by_key) - len(self.disabled_failure_keys)
            self.retry_button.setEnabled(idle and active > 0)
            self.failure_retry_button.setEnabled(idle and active > 0)
            self.failure_disable_button.setEnabled(
                idle and bool(self.failure_variants_by_key)
            )
            self.failure_enable_button.setEnabled(
                idle and bool(self.disabled_failure_keys)
            )

        def _append_full_output(self, text: str) -> None:
            cursor = self.full_log.textCursor()
            cursor.movePosition(cursor.MoveOperation.End)
            cursor.insertText(text)
            self.full_log.setTextCursor(cursor)
            self.full_log.ensureCursorVisible()

        def _update_case(
            self,
            key: str,
            status: str,
            detail: str,
            duration: float,
            evidence: str,
        ) -> None:
            prior_status = self.status_item_by_key.get(key)
            if prior_status is None:
                return
            row = prior_status.row()
            prior = self.live_status_by_key.get(key)
            if prior is not None and prior != status:
                self.live_status_counts[prior] -= 1
                if self.live_status_counts[prior] <= 0:
                    del self.live_status_counts[prior]
                self.live_status_counts[status] += 1
                self.live_status_by_key[key] = status
            self.case_table.setItem(
                row,
                self.PROGRESS_COLUMN,
                QTableWidgetItem(row_progress_text(status)),
            )
            status_item = QTableWidgetItem(status)
            self._color_status(status_item, status)
            self.case_table.setItem(row, self.STATUS_COLUMN, status_item)
            self.status_item_by_key[key] = status_item
            action_item = QTableWidgetItem(row_action_text(status, key in self.variant_by_key))
            if action_item.text():
                action_item.setForeground(QColor("#70a7ff"))
                action_item.setToolTip(
                    "Cancelar únicamente esta variante; la corrida continuará"
                    if status == "RUNNING"
                    else "Ejecutar solamente esta variante exacta"
                )
            self.case_table.setItem(row, self.ACTION_COLUMN, action_item)
            self.case_table.setItem(
                row,
                self.DURATION_COLUMN,
                QTableWidgetItem(
                    f"{duration:.1f} s"
                    if duration or status in {"RUNNING", "COMPILING"}
                    else ""
                ),
            )
            merged = detail
            if evidence:
                merged = f"{detail} | {evidence}" if detail else evidence
            variant = self.variant_by_key.get(key)
            if variant is not None and status in RESULT_STATUSES:
                self.current_outcomes_by_key[key] = CaseOutcome(
                    key=key,
                    case_id=variant.case.case_id,
                    contract_id=variant.case.contract_id,
                    component=variant.case.component,
                    wave=variant.case.wave,
                    build_mode=variant.build_mode,
                    optimization=variant.optimization,
                    status=status,
                    duration_seconds=duration,
                    detail=detail,
                    evidence=evidence,
                    profile=variant.case.profile,
                )
            self.case_table.setItem(
                row, self.DETAIL_COLUMN, QTableWidgetItem(merged)
            )
            self._apply_status_filter_to_row(row, status)
            position = self.variant_position_by_key.get(key, 0)
            prefix = f"{position:,}/{len(self.visible_variants):,}" if position else "Actual"
            self.execution_activity.setText(
                f"{prefix} · {status}: {key} — {merged or 'sin salida todavía'}"
            )
            self._refresh_live_counts()
            if not self.case_table.isRowHidden(row):
                self.case_table.scrollToItem(status_item)

        def _update_progress(self, done: int, total: int) -> None:
            self.progress.setRange(0, max(1, total))
            self.progress.setValue(done)
            self.progress.setFormat(f"{done:,}/{total:,} pruebas (%p%)")
            self.statusBar().showMessage(f"Ejecutando {done:,}/{total:,}")

        def _update_preparation(
            self, phase: str, done: int, total: int, detail: str
        ) -> None:
            """Project producer preparation without inventing synthetic test rows."""
            self.progress.setRange(0, max(1, total))
            self.progress.setValue(done)
            self.progress.setFormat(f"{phase}: {done:,}/{total:,} targets (%p%)")
            self.execution_activity.setText(f"{phase} — {detail}")
            self.statusBar().showMessage(f"{phase}: {done:,}/{total:,}")

        def _refresh_live_counts(self) -> None:
            """Render exact live state totals without rescanning the complete table."""
            order = (
                "LISTO",
                "PENDIENTE",
                "FILTRADO",
                "COMPILING",
                "RUNNING",
                "PASS",
                "FAIL",
                "BLOCKED",
                "INFRASTRUCTURE_ERROR",
                "CANCELLED",
                "TIMEOUT",
                "NOT_RUN",
            )
            parts = [
                f"{name}: {self.live_status_counts[name]:,}"
                for name in order
                if self.live_status_counts[name]
            ]
            self.execution_counts.setText(" · ".join(parts) if parts else "Sin resultados")

        def _suite_finished(
            self,
            outcomes: object,
            cancelled: bool,
            evidence: str,
            preparation: object,
        ) -> None:
            typed = list(outcomes) if isinstance(outcomes, list) else []
            prepared_failure = (
                preparation if isinstance(preparation, PreparationFailure) else None
            )
            failed_keys = {
                item.key for item in typed if item.status in TERMINAL_FAILURES
            }
            failed = (
                bool(failed_keys)
                or evidence.startswith("ERROR:")
                or prepared_failure is not None
            )
            self.last_preparation_variants = (
                self.visible_variants if prepared_failure is not None else ()
            )
            current_failures = retry_variants(self.visible_variants, typed)
            current_keys = {variant.key for variant in self.visible_variants}
            outcome_by_key = {item.key: item for item in typed}
            self.current_outcomes_by_key.update(outcome_by_key)
            for key in current_keys:
                self.failure_variants_by_key.pop(key, None)
                self.failure_outcomes_by_key.pop(key, None)
                self.disabled_failure_keys.discard(key)
            for variant in current_failures:
                self.failure_variants_by_key[variant.key] = variant
                self.failure_outcomes_by_key[variant.key] = outcome_by_key[variant.key]
            self.disabled_failure_keys.intersection_update(self.failure_variants_by_key)
            self.last_failed_variants = tuple(
                self.failure_variants_by_key[key]
                for key in sorted(self.failure_variants_by_key)
            )
            self._populate_failure_table()
            self._render_error_report(
                typed, cancelled, evidence, prepared_failure
            )
            completed = len(typed)
            total = len(self.visible_variants)
            self.progress.setRange(0, max(1, total))
            self.progress.setValue(min(completed, total))
            if prepared_failure is not None:
                label = (
                    "PREPARACIÓN CANCELADA"
                    if prepared_failure.status == "CANCELLED"
                    else "PREPARACIÓN FALLIDA"
                )
                self.progress.setValue(0)
                self.progress.setFormat(f"0/{total:,} pruebas — {label} (0%)")
                self.execution_activity.setText(
                    f"{label}: {prepared_failure.stage} — {prepared_failure.detail}"
                )
            else:
                self.progress.setFormat(
                    f"{completed:,}/{total:,} pruebas — TERMINADO (%p%)"
                )
                self.execution_activity.setText(
                    "Ejecución cancelada" if cancelled else "Ejecución terminada"
                )
            self.case_table.setSortingEnabled(True)
            self.worker = None
            self._set_busy(False)
            self._refresh_history()
            self.tabs.setCurrentIndex(self.results_tab_index)
            if prepared_failure is not None or evidence.startswith("ERROR:"):
                self.result_tabs.setCurrentIndex(1)
            else:
                self.result_tabs.setCurrentIndex(
                    self.failure_tab_index if failed else 0
                )

        def _release_finished_suite_worker(self) -> None:
            """Leave the GUI idle even if terminal result rendering raises."""
            if self.worker is self.sender():
                self.worker = None
                self._set_busy(False)

        def _render_error_report(
            self,
            outcomes: Sequence[CaseOutcome],
            cancelled: bool,
            evidence: str,
            preparation: PreparationFailure | None = None,
        ) -> None:
            self.error_report.setPlainText(
                condensed_error_report(outcomes, cancelled, evidence, preparation)
            )

        def _cancel_active(self) -> None:
            if self.worker is not None:
                self.worker.cancel()
                self.statusBar().showMessage("Cancelación solicitada al árbol activo")

        def _set_busy(self, busy: bool) -> None:
            self.compiler_configure_button.setEnabled(not busy)
            self.compiler_build_button.setEnabled(not busy)
            self.clean_compiler_button.setEnabled(not busy)
            self.clean_test_tree_button.setEnabled(not busy)
            self.configure_button.setEnabled(not busy)
            self.build_button.setEnabled(not busy)
            self.run_button.setEnabled(not busy)
            self.preview_button.setEnabled(not busy)
            self.unit_run_button.setEnabled(not busy)
            self.unit_preview_button.setEnabled(not busy)
            self.baseline_run_button.setEnabled(not busy)
            self.baseline_preview_button.setEnabled(not busy)
            self.deploy_run_button.setEnabled(not busy)
            self.deploy_preview_button.setEnabled(not busy)
            self.deploy_mode.setEnabled(not busy)
            self.save_button.setEnabled(not busy)
            self.detect_button.setEnabled(not busy)
            self.receipt_button.setEnabled(not busy)
            self.history_refresh_button.setEnabled(not busy)
            for page in self.history_run_pages.values():
                rerun_all = page.findChild(QPushButton, "history_rerun_all")
                rerun_failures = page.findChild(
                    QPushButton, "history_rerun_failures"
                )
                rerun_selected = page.findChild(
                    QPushButton, "history_rerun_selected"
                )
                if rerun_all is not None:
                    rerun_all.setEnabled(not busy)
                if rerun_failures is not None:
                    rerun_failures.setEnabled(
                        not busy
                        and bool(rerun_failures.property("hasRetryableFailures"))
                    )
                if rerun_selected is not None:
                    rerun_selected.setEnabled(
                        not busy
                        and bool(
                            rerun_selected.property("hasExecutableSelection")
                        )
                    )
            self.clear_results_button.setEnabled(not busy)
            infrastructure_busy = busy and bool(self.active_operation)
            self.infrastructure_cancel_button.setEnabled(infrastructure_busy)
            suite_busy = busy and isinstance(self.worker, SuiteWorker)
            self.cancel_button.setEnabled(
                suite_busy and self.active_suite_surface == "external"
            )
            self.unit_cancel_button.setEnabled(
                suite_busy and self.active_suite_surface == "unit"
            )
            self.baseline_cancel_button.setEnabled(
                busy and self.active_release_operation == "baseline"
            )
            self.deploy_cancel_button.setEnabled(
                busy and self.active_release_operation == "deploy"
            )
            if busy:
                self.retry_button.setEnabled(False)
                self.preparation_retry_button.setEnabled(False)
                self.failure_retry_button.setEnabled(False)
                self.failure_disable_button.setEnabled(False)
                self.failure_enable_button.setEnabled(False)
            else:
                self._refresh_failure_controls()
                self.unit_retry_button.setEnabled(bool(self.unit_last_failed_variants))
                self.preparation_retry_button.setEnabled(
                    bool(self.last_preparation_variants)
                )
                self._preview_history_selection()
            self.statusBar().showMessage("Operación activa" if busy else "Listo")

        def _clear_current_results(self) -> None:
            """Reset only the live workspace; historical result tabs stay untouched."""
            if self.worker is not None:
                return
            self.failure_outcomes_by_key.clear()
            self.failure_variants_by_key.clear()
            self.current_outcomes_by_key.clear()
            self.disabled_failure_keys.clear()
            self.last_failed_variants = ()
            self.last_preparation_variants = ()
            self.preparation_retry_button.setEnabled(False)
            self.full_log.clear()
            self.error_report.clear()
            for index in range(self.result_tabs.count() - 1, 2, -1):
                detail_page = self.result_tabs.widget(index)
                self.result_tabs.removeTab(index)
                detail_page.deleteLater()
            all_statuses_index = self.status_filter_combo.findData("ALL")
            if all_statuses_index >= 0:
                self.status_filter_combo.setCurrentIndex(all_statuses_index)
            self._populate_failure_table()
            self._preview_plan()
            self.execution_activity.setText("Preparada; ninguna prueba ha iniciado")
            self.statusBar().showMessage(
                "Resultados actuales limpiados; lista para una nueva corrida"
            )

        def _refresh_history(self) -> None:
            selected_path = self._selected_history_path()
            if not HISTORY_ROOT.is_dir():
                self.history_table.setRowCount(0)
                self.history_load_button.setEnabled(False)
                self.history_open_folder_button.setEnabled(False)
                self.history_preview_summary.setPlainText(
                    f"No existe todavía el directorio de historial: {HISTORY_ROOT}"
                )
                self.history_preview_log.clear()
                self.history_preview_report.clear()
                return
            records: list[tuple[tuple[str, ...], tuple[str, str, str, str, str]]] = []
            for root in sorted(HISTORY_ROOT.iterdir(), reverse=True):
                if not root.is_dir():
                    continue
                record = history_record(root)
                records.append((self._history_display_values(root, record), record))
            self.history_table.setSortingEnabled(False)
            self.history_table.clearContents()
            self.history_table.setRowCount(len(records))
            selected_row = 0
            for row, (values, record) in enumerate(records):
                for column, value in enumerate(values):
                    item = QTableWidgetItem(value)
                    item.setData(Qt.ItemDataRole.UserRole, record[4])
                    if column == 1:
                        self._color_status(item, record[2])
                    self.history_table.setItem(row, column, item)
                if selected_path is not None and record[4] == str(selected_path):
                    selected_row = row
            self.history_table.setSortingEnabled(True)
            if records:
                path_to_select = (
                    str(selected_path)
                    if selected_path is not None
                    else records[selected_row][1][4]
                )
                for row in range(self.history_table.rowCount()):
                    item = self.history_table.item(row, self.HISTORY_PATH_COLUMN)
                    if item is not None and item.text() == path_to_select:
                        self.history_table.selectRow(row)
                        break
            else:
                self.history_preview_summary.setPlainText(
                    "No hay corridas guardadas todavía."
                )
                self.history_preview_log.clear()
                self.history_preview_report.clear()
                self.history_load_button.setEnabled(False)
                self.history_open_folder_button.setEnabled(False)

        def _history_display_values(
            self,
            root: Path,
            record: tuple[str, str, str, str, str],
        ) -> tuple[str, ...]:
            """Describe a stored run in user-facing terms without executing it."""
            run_id, kind, status, finished_at, path = record
            display_time = finished_at.replace("T", " ") if finished_at else "Sin cierre"
            result_summary = "Sin resultados completos"
            configuration = "No disponible"
            test_count = "0"
            try:
                metadata_path = root / "metadata.json"
                results_path = root / "results.json"
                metadata = (
                    json.loads(
                        metadata_path.read_text(encoding="utf-8"),
                        object_pairs_hook=unique_json_object,
                    )
                    if metadata_path.is_file()
                    else {}
                )
                payload = (
                    json.loads(
                        results_path.read_text(encoding="utf-8"),
                        object_pairs_hook=unique_json_object,
                    )
                    if results_path.is_file()
                    else {}
                )
                raw_counts = payload.get("counts", {})
                if isinstance(raw_counts, dict) and raw_counts:
                    order = (
                        "PASS",
                        "FAIL",
                        "INFRASTRUCTURE_ERROR",
                        "BLOCKED",
                        "CANCELLED",
                    )
                    projected_counts: Counter[str] = Counter()
                    for name, count in raw_counts.items():
                        if isinstance(name, str) and isinstance(count, int):
                            projected_counts[normalize_observed_status(name)] += count
                    result_summary = " · ".join(
                        f"{name}: {projected_counts[name]:,}"
                        for name in order
                        if projected_counts[name] > 0
                    )
                raw_results = payload.get("results")
                if isinstance(raw_results, list):
                    test_count = f"{len(raw_results):,} terminales"
                selection = metadata.get("selection")
                if isinstance(selection, dict):
                    scope = str(selection.get("scope", ""))
                    value = str(selection.get("value", ""))
                    profile = str(selection.get("profile", ""))
                    mode = str(selection.get("build_mode", ""))
                    optimization = str(selection.get("optimization", ""))
                    scope_text = f"{scope} {value}".strip()
                    configuration = " · ".join(
                        part
                        for part in (scope_text, profile, mode, optimization)
                        if part
                    )
                elif isinstance(metadata, dict):
                    configuration = str(
                        metadata.get("operation") or metadata.get("origin") or kind
                    )
            except (OSError, json.JSONDecodeError, ValueError, TypeError):
                result_summary = "La evidencia no se pudo interpretar"
            kind_label = "Suite de pruebas" if kind == "phase-2-suite" else kind
            return (
                display_time,
                status,
                result_summary,
                configuration,
                test_count,
                kind_label,
                run_id,
                path,
            )

        def _selected_history_path(self) -> Path | None:
            """Return the evidence directory selected in the history launcher."""
            row = self.history_table.currentRow()
            if row < 0:
                return None
            item = self.history_table.item(row, self.HISTORY_PATH_COLUMN)
            if item is None or not item.text():
                return None
            return Path(item.text())

        def _preview_history_selection(self) -> None:
            """Show identity, summary and log for a run before opening its full tab."""
            root = self._selected_history_path()
            if root is None:
                self.history_load_button.setEnabled(False)
                self.history_open_folder_button.setEnabled(False)
                return
            record = history_record(root)
            values = self._history_display_values(root, record)
            summary = [
                f"Corrida: {record[0]}",
                f"Fecha y hora: {values[0]}",
                f"Estado: {record[2]}",
                f"Tipo: {values[5]}",
                f"Configuración: {values[3]}",
                f"Pruebas: {values[4]}",
                f"Resultados: {values[2]}",
                f"Evidencia: {root}",
            ]
            if record[1] != "phase-2-suite":
                summary.append(
                    "Esta fila no es una corrida completa de pruebas de Fase 2; "
                    "puede consultarse, pero no abrirse como batería reejecutable."
                )
            elif record[2] in {"INCOMPLETE", "MALFORMED"}:
                summary.append(
                    "La corrida está incompleta o dañada y no puede reejecutarse."
                )
            self.history_preview_summary.setPlainText("\n".join(summary))
            log_path = root / "full.log"
            self.history_preview_log.setPlainText(
                log_path.read_text(encoding="utf-8", errors="replace")
                if log_path.is_file()
                else "Esta corrida no conserva full.log."
            )
            report_path = root / "summary.md"
            self.history_preview_report.setPlainText(
                report_path.read_text(encoding="utf-8", errors="replace")
                if report_path.is_file()
                else "Esta corrida no conserva summary.md."
            )
            idle = self.worker is None
            can_open = (
                record[1] == "phase-2-suite"
                and record[2] not in {"INCOMPLETE", "MALFORMED"}
            )
            self.history_load_button.setEnabled(idle and can_open)
            self.history_open_folder_button.setEnabled(idle)

        def _load_history_from_table(self) -> None:
            root = self._selected_history_path()
            if root is not None:
                self._load_history_path(root)

        def _load_history_path(self, root: Path) -> None:
            """Open one immutable historical run in its own Results tab."""
            if self.worker is not None:
                return
            try:
                loaded = load_history_execution(root, self.catalog)
                key = os.path.normcase(str(loaded.root))
                existing = self.history_run_pages.get(key)
                if existing is not None:
                    index = self.history_workspace_tabs.indexOf(existing)
                    if index >= 0:
                        self.tabs.setCurrentIndex(self.history_top_tab_index)
                        self.history_workspace_tabs.setCurrentIndex(index)
                        self.statusBar().showMessage(
                            f"Ejecución histórica enfocada: {loaded.root.name}"
                        )
                        return
                    self.history_run_pages.pop(key, None)
                page = self._create_history_run_page(loaded, key)
                label = f"{loaded.root.name[:15]} · {loaded.status}"
                index = self.history_workspace_tabs.addTab(page, label)
                self.history_workspace_tabs.setTabToolTip(index, str(loaded.root))
                self.history_run_pages[key] = page
                self.tabs.setCurrentIndex(self.history_top_tab_index)
                self.history_workspace_tabs.setCurrentIndex(index)
                self.statusBar().showMessage(
                    f"Ejecución histórica abierta: {loaded.root.name}"
                )
            except Exception as error:
                self._show_error("No se pudo cargar la ejecución", error)

        def _create_history_run_page(
            self, loaded: HistoricalExecution, page_key: str
        ) -> QWidget:
            """Build an actionable projection of one immutable saved run."""
            page = QWidget()
            layout = QVBoxLayout(page)
            record = history_record(loaded.root)
            counts = Counter(outcome.status for outcome in loaded.outcomes)
            selection = loaded.metadata["selection"]
            header = QLabel(
                f"{loaded.root.name} · {loaded.status} · {record[3]}\n"
                f"{selection['scope']} {selection['value']} · {selection['profile']} · "
                f"{selection['build_mode']} · {selection['optimization']} · "
                + " · ".join(
                    f"{status}: {counts[status]:,}"
                    for status in sorted(counts)
                )
            )
            header.setWordWrap(True)
            layout.addWidget(header)

            run_tabs = QTabWidget()
            run_tabs.setObjectName("history_run_sections")
            tests_page = QWidget()
            tests_layout = QVBoxLayout(tests_page)
            actions = QHBoxLayout()
            rerun_selected_button = QPushButton("Reejecutar prueba seleccionada")
            rerun_selected_button.setObjectName("history_rerun_selected")
            rerun_selected_button.setEnabled(False)
            rerun_failures_button = QPushButton("Reejecutar fallos")
            rerun_failures_button.setObjectName("history_rerun_failures")
            has_retryable_failures = bool(
                retry_variants(loaded.variants, loaded.outcomes)
            )
            rerun_failures_button.setProperty(
                "hasRetryableFailures", has_retryable_failures
            )
            rerun_failures_button.setEnabled(has_retryable_failures)
            rerun_all_button = QPushButton("Reejecutar corrida completa")
            rerun_all_button.setObjectName("history_rerun_all")
            failures_button = QPushButton("Mostrar sólo fallos")
            open_button = QPushButton("Abrir carpeta de evidencia")
            close_button = QPushButton("Cerrar corrida")
            actions.addWidget(rerun_selected_button)
            actions.addWidget(rerun_failures_button)
            actions.addWidget(rerun_all_button)
            actions.addWidget(failures_button)
            actions.addStretch(1)
            actions.addWidget(open_button)
            actions.addWidget(close_button)
            tests_layout.addLayout(actions)

            filters = QHBoxLayout()
            status_combo = QComboBox()
            for label, value in (
                ("Todos los estados", "ALL"),
                ("Fallos", "FAILURES"),
                ("PASS", "PASS"),
                ("FAIL", "FAIL"),
                ("BLOCKED", "BLOCKED"),
                ("INFRASTRUCTURE_ERROR", "INFRASTRUCTURE_ERROR"),
                ("CANCELLED", "CANCELLED"),
                ("TIMEOUT", "TIMEOUT"),
                ("NOT_RUN / BLOQUEADO_POR", "NOT_RUN"),
                ("PENDIENTE", "PENDIENTE"),
                ("FILTRADO", "FILTRADO"),
            ):
                status_combo.addItem(label, value)
            component_combo = QComboBox()
            component_combo.addItem("Todos los componentes", "ALL")
            for component in sorted({case.component for case in loaded.cases}):
                component_combo.addItem(component, component)
            time_combo = QComboBox()
            for label, value in (
                ("Cualquier momento", "ALL"),
                ("Primer minuto", "LT60"),
                ("1 a 5 min", "60_300"),
                ("5 a 15 min", "300_900"),
                ("15 a 30 min", "900_1800"),
                ("Después de 30 min", "GE1800"),
                ("Sin hora", "NONE"),
            ):
                time_combo.addItem(label, value)
            duration_combo = QComboBox()
            for label, value in (
                ("Cualquier duración", "ALL"),
                ("Menos de 1 s", "LT1"),
                ("1 a 5 s", "1_5"),
                ("5 a 30 s", "5_30"),
                ("30 a 60 s", "30_60"),
                ("60 s o más", "GE60"),
                ("Sin duración", "NONE"),
            ):
                duration_combo.addItem(label, value)
            search_edit = QLineEdit()
            search_edit.setPlaceholderText("Buscar caso, contrato, componente o detalle")
            visible_label = QLabel()
            filters.addWidget(QLabel("Estado"))
            filters.addWidget(status_combo)
            filters.addWidget(QLabel("Componente"))
            filters.addWidget(component_combo)
            filters.addWidget(QLabel("Tiempo"))
            filters.addWidget(time_combo)
            filters.addWidget(QLabel("Duración"))
            filters.addWidget(duration_combo)
            filters.addWidget(search_edit, 1)
            tests_layout.addLayout(filters)
            tests_layout.addWidget(visible_label)

            history_headers = (
                *self.TABLE_HEADERS[: self.DURATION_COLUMN],
                "Hora",
                *self.TABLE_HEADERS[self.DURATION_COLUMN :],
            )
            table = QTableWidget(0, len(history_headers))
            table.setObjectName("history_test_table")
            table.setHorizontalHeaderLabels(history_headers)
            table.setAlternatingRowColors(True)
            table.setSelectionBehavior(QTableWidget.SelectionBehavior.SelectRows)
            table.setSelectionMode(QAbstractItemView.SelectionMode.SingleSelection)
            table.setEditTriggers(QTableWidget.EditTrigger.NoEditTriggers)
            table.setSortingEnabled(False)
            table.setUpdatesEnabled(False)
            table.horizontalHeader().setSectionResizeMode(
                QHeaderView.ResizeMode.Interactive
            )
            for column, width in enumerate(
                (55, 185, 165, 175, 70, 55, 75, 175, 90, 125, 120, 185, 90)
            ):
                table.horizontalHeader().resizeSection(column, width)
            table.horizontalHeader().setStretchLastSection(True)

            event_text = ""
            finished_at_by_key: dict[str, tuple[str, float | None]] = {}
            run_started_at: datetime | None = None
            events_path = loaded.root / "events.jsonl"
            if events_path.is_file():
                event_text = events_path.read_text(encoding="utf-8", errors="replace")
                for line in event_text.splitlines():
                    if not line.strip():
                        continue
                    event = json.loads(line, object_pairs_hook=unique_json_object)
                    timestamp = event.get("timestamp")
                    if not isinstance(timestamp, str):
                        continue
                    parsed = datetime.fromisoformat(timestamp)
                    if event.get("kind") == "RUN_STARTED":
                        run_started_at = parsed
                    elif event.get("kind") == "CASE_FINISHED":
                        payload = event.get("payload")
                        if not isinstance(payload, dict):
                            continue
                        event_key = payload.get("key")
                        if not isinstance(event_key, str):
                            continue
                        elapsed = (
                            (parsed - run_started_at).total_seconds()
                            if run_started_at is not None
                            else None
                        )
                        finished_at_by_key[event_key] = (timestamp, elapsed)

            outcome_by_key = {outcome.key: outcome for outcome in loaded.outcomes}
            variant_by_key = {variant.key: variant for variant in loaded.variants}
            case_by_id = {case.case_id: case for case in loaded.cases}
            executable_ids = {variant.case.case_id for variant in loaded.variants}
            rows: list[
                tuple[
                    CaseRecord,
                    str,
                    str,
                    str,
                    str,
                    float | None,
                    str,
                    str,
                    float | None,
                    bool,
                ]
            ] = []
            for variant in loaded.variants:
                outcome = outcome_by_key[variant.key]
                detail = outcome.detail
                if outcome.evidence:
                    detail = (
                        f"{detail} | {outcome.evidence}"
                        if detail
                        else outcome.evidence
                    )
                timestamp, elapsed = finished_at_by_key.get(variant.key, ("", None))
                rows.append(
                    (
                        variant.case,
                        variant.build_mode,
                        variant.optimization,
                        variant.key,
                        outcome.status,
                        outcome.duration_seconds,
                        detail,
                        timestamp,
                        elapsed,
                        True,
                    )
                )
            for case in loaded.cases:
                if not case.executable:
                    rows.append(
                        (
                            case,
                            "NA",
                            "NA",
                            f"{case.case_id}|NA|NA",
                            "PENDIENTE",
                            None,
                            case.disposition,
                            "",
                            None,
                            False,
                        )
                    )
                elif case.case_id not in executable_ids:
                    rows.append(
                        (
                            case,
                            "NA",
                            "NA",
                            f"{case.case_id}|FILTERED|FILTERED",
                            "FILTRADO",
                            None,
                            "la combinación Build/Opt elegida no está soportada por el caso",
                            "",
                            None,
                            False,
                        )
                    )

            table.setRowCount(len(rows))
            history_time_column = self.DURATION_COLUMN
            history_duration_column = self.DURATION_COLUMN + 1
            history_detail_column = self.DETAIL_COLUMN + 1
            for row_index, (
                case,
                mode,
                opt,
                variant_key,
                status,
                duration,
                detail,
                timestamp,
                elapsed,
                executable,
            ) in enumerate(rows):
                values = (
                    str(case.wave),
                    case.component,
                    case.contract_id,
                    case.case_id,
                    mode,
                    opt,
                    case.profile,
                    case.executor,
                    row_progress_text(status),
                    status,
                    row_action_text(status, executable),
                    timestamp.replace("T", " ") if timestamp else "",
                    "",
                    detail,
                )
                for column, value in enumerate(values):
                    item = QTableWidgetItem(value)
                    if column == self.CASE_COLUMN:
                        item.setData(Qt.ItemDataRole.UserRole, variant_key)
                        item.setToolTip(
                            "Doble clic para abrir el escenario, el oráculo y la salida completa"
                        )
                    elif column == self.STATUS_COLUMN:
                        self._color_status(item, status)
                    elif column == self.ACTION_COLUMN and executable:
                        item.setForeground(QColor("#70a7ff"))
                        item.setToolTip("Crear una corrida nueva de esta variante exacta")
                    elif column == history_time_column:
                        item.setData(Qt.ItemDataRole.UserRole, elapsed)
                    elif column == history_duration_column:
                        item.setData(Qt.ItemDataRole.UserRole, duration)
                        if duration is not None:
                            item.setData(
                                Qt.ItemDataRole.DisplayRole, round(duration, 6)
                            )
                            item.setToolTip(f"{duration:.6f} segundos")
                    table.setItem(row_index, column, item)
            table.setUpdatesEnabled(True)
            table.setSortingEnabled(True)
            tests_layout.addWidget(table, 1)

            def matches_bucket(value: float | None, bucket: str, kind: str) -> bool:
                if bucket == "ALL":
                    return True
                if bucket == "NONE":
                    return value is None
                if value is None:
                    return False
                if kind == "time":
                    if bucket == "LT60":
                        return value < 60.0
                    if bucket == "60_300":
                        return 60.0 <= value < 300.0
                    if bucket == "300_900":
                        return 300.0 <= value < 900.0
                    if bucket == "900_1800":
                        return 900.0 <= value < 1800.0
                    return value >= 1800.0
                if bucket == "LT1":
                    return value < 1.0
                if bucket == "1_5":
                    return 1.0 <= value < 5.0
                if bucket == "5_30":
                    return 5.0 <= value < 30.0
                if bucket == "30_60":
                    return 30.0 <= value < 60.0
                return value >= 60.0

            def apply_filters() -> None:
                status_filter = str(status_combo.currentData())
                component_filter = str(component_combo.currentData())
                time_filter = str(time_combo.currentData())
                duration_filter = str(duration_combo.currentData())
                needle = search_edit.text().strip().casefold()
                visible = 0
                for row_index in range(table.rowCount()):
                    status_item = table.item(row_index, self.STATUS_COLUMN)
                    component_item = table.item(row_index, 1)
                    time_item = table.item(row_index, history_time_column)
                    duration_item = table.item(row_index, history_duration_column)
                    status = status_item.text() if status_item is not None else ""
                    component = (
                        component_item.text() if component_item is not None else ""
                    )
                    elapsed = (
                        time_item.data(Qt.ItemDataRole.UserRole)
                        if time_item is not None
                        else None
                    )
                    duration_value = (
                        duration_item.data(Qt.ItemDataRole.UserRole)
                        if duration_item is not None
                        else None
                    )
                    searchable = " ".join(
                        table.item(row_index, column).text()
                        for column in (
                            0,
                            1,
                            2,
                            3,
                            4,
                            5,
                            6,
                            7,
                            self.STATUS_COLUMN,
                            history_detail_column,
                        )
                        if table.item(row_index, column) is not None
                    ).casefold()
                    show = (
                        status_filter_matches(status_filter, status)
                        and (component_filter == "ALL" or component == component_filter)
                        and matches_bucket(elapsed, time_filter, "time")
                        and matches_bucket(duration_value, duration_filter, "duration")
                        and (not needle or needle in searchable)
                    )
                    table.setRowHidden(row_index, not show)
                    visible += int(show)
                visible_label.setText(
                    f"Mostrando {visible:,} de {table.rowCount():,} filas guardadas; "
                    "los encabezados permiten ordenar, incluida la duración numérica."
                )

            action_timer = QTimer(table)
            action_timer.setSingleShot(True)
            pending_action: dict[str, str | float | None] = {
                "key": None,
                "suppressed_until": 0.0,
            }

            def execute_pending_action() -> None:
                variant_key = pending_action["key"]
                pending_action["key"] = None
                if not isinstance(variant_key, str) or self.worker is not None:
                    return
                variant = variant_by_key.get(variant_key)
                if variant is not None:
                    self._start_historical_variant(variant)

            action_timer.timeout.connect(execute_pending_action)

            def run_exact(row: int, column: int) -> None:
                if (
                    column != self.ACTION_COLUMN
                    or self.worker is not None
                    or time.monotonic()
                    < float(pending_action["suppressed_until"] or 0.0)
                ):
                    return
                item = table.item(row, self.CASE_COLUMN)
                variant_key = (
                    item.data(Qt.ItemDataRole.UserRole) if item is not None else None
                )
                if isinstance(variant_key, str) and variant_key in variant_by_key:
                    pending_action["key"] = variant_key
                    action_timer.start(QApplication.doubleClickInterval() + 50)

            def open_detail(row: int, _column: int) -> None:
                action_timer.stop()
                pending_action["key"] = None
                pending_action["suppressed_until"] = time.monotonic() + 0.5
                case_item = table.item(row, self.CASE_COLUMN)
                if case_item is None:
                    return
                case = case_by_id.get(case_item.text())
                variant_key = case_item.data(Qt.ItemDataRole.UserRole)
                if case is None or not isinstance(variant_key, str):
                    return
                self._open_case_detail_tab(
                    run_tabs,
                    case,
                    variant_by_key.get(variant_key),
                    outcome_by_key.get(variant_key),
                    loaded.root,
                )

            def selected_variant() -> RunVariant | None:
                row = table.currentRow()
                if row < 0:
                    return None
                item = table.item(row, self.CASE_COLUMN)
                variant_key = (
                    item.data(Qt.ItemDataRole.UserRole) if item is not None else None
                )
                return (
                    variant_by_key.get(variant_key)
                    if isinstance(variant_key, str)
                    else None
                )

            def update_selected_action() -> None:
                has_executable_selection = selected_variant() is not None
                rerun_selected_button.setProperty(
                    "hasExecutableSelection", has_executable_selection
                )
                rerun_selected_button.setEnabled(
                    self.worker is None and has_executable_selection
                )

            def rerun_selected() -> None:
                variant = selected_variant()
                if variant is not None:
                    self._start_historical_variant(variant)

            for widget_signal in (
                status_combo.currentIndexChanged,
                component_combo.currentIndexChanged,
                time_combo.currentIndexChanged,
                duration_combo.currentIndexChanged,
            ):
                widget_signal.connect(lambda _value: apply_filters())
            search_edit.textChanged.connect(lambda _text: apply_filters())
            table.cellClicked.connect(run_exact)
            table.cellDoubleClicked.connect(open_detail)
            table.itemSelectionChanged.connect(update_selected_action)
            rerun_selected_button.clicked.connect(rerun_selected)
            rerun_failures_button.clicked.connect(
                lambda: self._start_historical_failures(loaded)
            )
            rerun_all_button.clicked.connect(
                lambda: self._start_historical_run(loaded)
            )
            failures_button.clicked.connect(
                lambda: status_combo.setCurrentIndex(
                    max(0, status_combo.findData("FAILURES"))
                )
            )
            open_button.clicked.connect(
                lambda: QDesktopServices.openUrl(QUrl.fromLocalFile(str(loaded.root)))
            )
            close_button.clicked.connect(
                lambda: self._close_history_run_tab(page_key, page)
            )
            run_tabs.addTab(tests_page, f"Pruebas ({len(rows):,})")

            report_page = QWidget()
            report_layout = QVBoxLayout(report_page)
            report_intro = QLabel(
                f"Evidencia inmutable de {loaded.root.name}\n{loaded.root}"
            )
            report_intro.setWordWrap(True)
            report_layout.addWidget(report_intro)
            report_tabs = QTabWidget()
            report_tabs.setObjectName("history_report_sections")

            def add_text_tab(title: str, contents: str) -> None:
                view = QPlainTextEdit()
                view.setReadOnly(True)
                view.setPlainText(contents)
                report_tabs.addTab(view, title)

            summary_path = loaded.root / "summary.md"
            add_text_tab(
                "Resumen",
                summary_path.read_text(encoding="utf-8", errors="replace"),
            )
            add_text_tab(
                "Errores condensados",
                condensed_error_report(
                    loaded.outcomes,
                    loaded.status == "CANCELLED",
                    str(loaded.root),
                    loaded.preparation,
                ),
            )
            add_text_tab("Log completo", loaded.full_log)
            add_text_tab("Eventos", event_text)
            add_text_tab(
                "Resultados JSON",
                (loaded.root / "results.json").read_text(
                    encoding="utf-8", errors="replace"
                ),
            )
            add_text_tab(
                "Metadatos",
                json.dumps(loaded.metadata, ensure_ascii=False, indent=2),
            )
            report_layout.addWidget(report_tabs, 1)
            run_tabs.addTab(report_page, "Reporte")
            layout.addWidget(run_tabs, 1)
            apply_filters()
            return page

        def _close_history_run_tab(self, page_key: str, page: QWidget) -> None:
            """Close only the selected historical view; evidence remains on disk."""
            index = self.history_workspace_tabs.indexOf(page)
            if index >= 0:
                self.history_workspace_tabs.removeTab(index)
            self.history_run_pages.pop(page_key, None)

        def _start_historical_variant(self, variant: RunVariant) -> None:
            """Create a new exact run from a historical row without editing that tab."""
            if self.worker is not None:
                return
            self.scope_combo.setCurrentText("Caso")
            self._refresh_scope_values()
            self.scope_value.setCurrentText(variant.case.case_id)
            self.profile_combo.setCurrentText("Todos")
            self.execution_mode.setCurrentText(variant.build_mode)
            requested_optimization = (
                "O0" if variant.optimization == "NA" else variant.optimization
            )
            self.optimization_combo.setCurrentText(requested_optimization)
            self.visible_variants = (variant,)
            self._populate_plan((variant.case,), (variant,))
            self.tabs.setCurrentIndex(1)
            self._start_exact_variant(variant)

        def _start_historical_run(self, loaded: HistoricalExecution) -> None:
            """Create a new run from the exact selection stored by a prior run."""
            if self.worker is not None:
                return
            try:
                selection = loaded.metadata["selection"]
                scope = str(selection["scope"])
                self.scope_combo.setCurrentText(scope)
                self._refresh_scope_values()
                if scope != "Total":
                    self.scope_value.setCurrentText(str(selection["value"]))
                self.profile_combo.setCurrentText(str(selection["profile"]))
                self.execution_mode.setCurrentText(str(selection["build_mode"]))
                self.optimization_combo.setCurrentText(
                    str(selection["optimization"])
                )
                config = self._config_from_ui().validated(require_tools=False)
                config.save()
                if not loaded.variants:
                    raise ValueError(
                        "la corrida histórica no contiene variantes ejecutables"
                    )
                self._populate_plan(loaded.cases, loaded.variants)
                self.tabs.setCurrentIndex(1)
                self._run_variants(
                    config,
                    loaded.variants,
                    f"historical-full:{loaded.root.name}",
                )
            except Exception as error:
                self._show_error("No se pudo reejecutar la corrida completa", error)

        def _start_historical_failures(self, loaded: HistoricalExecution) -> None:
            """Create a new run from only the exact failures of a prior run."""
            if self.worker is not None:
                return
            try:
                variants = retry_variants(loaded.variants, loaded.outcomes)
                if not variants:
                    raise ValueError(
                        "la corrida histórica no contiene fallos reejecutables"
                    )
                selection = loaded.metadata["selection"]
                scope = str(selection["scope"])
                self.scope_combo.setCurrentText(scope)
                self._refresh_scope_values()
                if scope != "Total":
                    self.scope_value.setCurrentText(str(selection["value"]))
                self.profile_combo.setCurrentText(str(selection["profile"]))
                self.execution_mode.setCurrentText(str(selection["build_mode"]))
                self.optimization_combo.setCurrentText(
                    str(selection["optimization"])
                )
                config = self._config_from_ui().validated(require_tools=False)
                config.save()
                self._populate_plan(
                    tuple(variant.case for variant in variants), variants
                )
                self.tabs.setCurrentIndex(1)
                self._run_variants(
                    config,
                    variants,
                    f"historical-failures:{loaded.root.name}",
                )
            except Exception as error:
                self._show_error("No se pudieron reejecutar los fallos", error)

        def _open_history(self) -> None:
            root = self._selected_history_path()
            if root is not None:
                QDesktopServices.openUrl(QUrl.fromLocalFile(str(root)))

        @staticmethod
        def _color_status(item: QTableWidgetItem, status: str) -> None:
            colors = {
                "PASS": QColor("#73d673"),
                "RUNNING": QColor("#8cb4ff"),
                "COMPILING": QColor("#e6c76a"),
                "FAIL": QColor("#ff7777"),
                "BLOCKED": QColor("#d9955b"),
                "INFRASTRUCTURE_ERROR": QColor("#ff7777"),
                "CANCELLED": QColor("#b7b7b7"),
                "TIMEOUT": QColor("#ff7777"),
                "NOT_RUN": QColor("#ff7777"),
                "PENDIENTE": QColor("#d9b85f"),
                "LISTO": QColor("#8cb4ff"),
                "DESACTIVADA": QColor("#b7b7b7"),
                "PASSED": QColor("#73d673"),
                "FAILED": QColor("#ff7777"),
                "PREPARATION_FAILED": QColor("#ff7777"),
                "COMPLETED_WITH_KNOWN_DEFECTS": QColor("#f0a35e"),
                "INCOMPLETE": QColor("#b7b7b7"),
                "MALFORMED": QColor("#ff7777"),
            }
            color = colors.get(status)
            if color is not None:
                item.setForeground(color)

        def _show_error(self, title: str, error: Exception) -> None:
            QMessageBox.critical(self, title, str(error))

        def closeEvent(self, event: Any) -> None:
            if self.worker is not None:
                QMessageBox.warning(
                    self,
                    "Operación activa",
                    "Cancela la operación activa antes de cerrar NSC Builder 3.",
                )
                event.ignore()
                return
            event.accept()

    application = QApplication.instance() or QApplication(sys.argv)
    application.setApplicationName(APP_NAME)
    application.setStyle("Fusion")
    window = MainWindow()
    if smoke_only:
        expected_buttons = {
            "run_button": "Ejecutar pruebas",
            "cancel_button": "Cancelar corrida/árbol",
            "clear_results_button": "Limpiar resultados para nueva corrida",
            "clean_compiler_button": "Limpiar compilador",
            "history_load_button": "Abrir corrida completa en un tab",
            "failure_retry_button": "Reejecutar fallos activos",
            "preparation_retry_button": "Reintentar preparación/corrida",
            "failure_disable_button": "Desactivar seleccionados para reejecución",
            "failure_enable_button": "Reactivar seleccionados",
            "unit_run_button": "Ejecutar unitarias",
            "unit_cancel_button": "Cancelar corrida/árbol",
            "baseline_run_button": "Certificar / crear baseline",
            "baseline_cancel_button": "Cancelar certificación",
            "deploy_run_button": "Instalar para pruebas",
            "deploy_cancel_button": "Cancelar instalación",
        }
        for attribute, expected_text in expected_buttons.items():
            button = getattr(window, attribute, None)
            if button is None or button.text() != expected_text:
                raise RuntimeError(
                    f"GUI smoke missing {attribute} with text {expected_text!r}"
                )
        window._set_busy(True)
        if window.clean_compiler_button.isEnabled():
            raise RuntimeError("GUI smoke allows compiler cleanup while a worker is active")
        window._set_busy(False)
        if window.result_tabs.tabText(window.failure_tab_index) != "Fallos exactos (0)":
            raise RuntimeError("GUI smoke missing the exact-failure result tab")
        primary_tabs = [
            window.tabs.tabText(index) for index in range(window.tabs.count())
        ]
        if primary_tabs != [
            "Construcción",
            "Pruebas externas",
            "Pruebas unitarias",
            "Resultados",
            "Historial",
            "Baseline",
            "Deploy",
        ]:
            raise RuntimeError(
                "GUI smoke missing the seven independent top-level workspaces"
            )
        unit_ids = [case.case_id for case in window.catalog.cases if case.layer == "unit"]
        if len(unit_ids) != len(set(unit_ids)) or not unit_ids:
            raise RuntimeError("GUI smoke found an empty or duplicated unit-test projection")
        if any(
            window.result_tabs.tabText(index) == "Historial"
            for index in range(window.result_tabs.count())
        ):
            raise RuntimeError("GUI smoke found history mixed into current results")
        if (
            window.history_workspace_tabs.tabText(
                window.history_launcher_tab_index
            )
            != "Corridas guardadas"
        ):
            raise RuntimeError("GUI smoke missing the history run-list panel")
        headers = [
            window.case_table.horizontalHeaderItem(index).text()
            for index in range(window.case_table.columnCount())
        ]
        if (
            "Progreso" not in headers
            or "Estado" not in headers
            or "Acción" not in headers
            or "Duración" not in headers
        ):
            raise RuntimeError("GUI smoke missing per-row progress columns")
        if window.case_table.contextMenuPolicy() != Qt.ContextMenuPolicy.CustomContextMenu:
            raise RuntimeError("GUI smoke missing exact-row context actions")
        if window.execution_activity is None or window.execution_counts is None:
            raise RuntimeError("GUI smoke missing live execution summaries")
        if window.status_filter_combo.count() < 10:
            raise RuntimeError("GUI smoke missing the primary status filters")
        if window.status_filter_combo.findData("TIMEOUT") < 0:
            raise RuntimeError("GUI smoke missing the TIMEOUT status filter")
        if window.status_filter_combo.findData("NOT_RUN") < 0:
            raise RuntimeError("GUI smoke missing the producer-blocked status filter")
        smoke_baseline_variants = expand_variants(
            window.catalog.cases, "Todos", "Todas"
        )
        window._populate_baseline_plan(smoke_baseline_variants)
        baseline_headers = [
            window.baseline_table.horizontalHeaderItem(index).text()
            for index in range(window.baseline_table.columnCount())
        ]
        if baseline_headers != [
            "Categoría",
            "Caso",
            "Componente",
            "Build",
            "Opt",
            "Estado",
            "Duración",
            "Detalle / evidencia",
        ]:
            raise RuntimeError("GUI smoke missing the categorized baseline table")
        if (
            window.baseline_table.rowCount() != 1
            or window.baseline_table.item(0, 0).text() != "Prueba unitaria"
            or "Prueba externa" not in window.baseline_category_summary.text()
        ):
            raise RuntimeError("GUI smoke missing canonical baseline categories")
        smoke_baseline_key = smoke_baseline_variants[0].key
        window._update_baseline_case(
            smoke_baseline_key,
            "PASS",
            "smoke baseline",
            0.25,
            "A:/nsc-devel/_nscbuilder3/smoke",
        )
        smoke_status = window.baseline_status_items[smoke_baseline_key]
        if (
            smoke_status.text() != "PASS"
            or window.baseline_table.item(smoke_status.row(), 6).text() != "0.2 s"
            or "smoke baseline"
            not in window.baseline_table.item(smoke_status.row(), 7).text()
        ):
            raise RuntimeError("GUI smoke cannot update a live baseline row")
        if not hasattr(SuiteWorker, "preparation_ready"):
            raise RuntimeError("GUI smoke missing producer preparation progress")
        window.close()
        return 0
    window.show()
    return application.exec()
