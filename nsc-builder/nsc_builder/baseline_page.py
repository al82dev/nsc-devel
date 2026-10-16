"""Independent baseline certification workspace for NSC Builder 3."""

from __future__ import annotations

import subprocess
from collections import defaultdict
from dataclasses import replace
from pathlib import Path
from typing import Any, Sequence

from PySide6.QtCore import Qt, QThread, Signal
from PySide6.QtWidgets import (
    QAbstractItemView,
    QFormLayout,
    QGroupBox,
    QHeaderView,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QMessageBox,
    QPlainTextEdit,
    QProgressBar,
    QPushButton,
    QTableWidget,
    QTableWidgetItem,
    QVBoxLayout,
    QWidget,
)

from .baseline import (
    BASELINE_OPTIMIZATIONS,
    BASELINE_PREFIX,
    BASELINE_PROMOTION_PREFIX,
    BASELINE_EXTERNAL_CATEGORY,
    BASELINE_UNIT_CATEGORY,
    BaselinePlan,
    baseline_test_category,
    create_baseline_plan,
    promote_certified_baseline,
    reset_baseline_staging,
    snapshot_baseline_comparison,
    stage_baseline_nslink,
    tree_fingerprint,
)
from .catalog import CanonicalCatalog, expand_variants
from .constants import TERMINAL_FAILURES
from .execution import ExecutionEngine
from .history import EvidenceStore, load_variant_timeout_budgets
from .models import CaseOutcome, PreparationFailure, RunVariant
from .paths import SOURCE_ROOT
from .process_control import ProcessController
from .runtime_support import make_run_id
from .utils import current_revision


class BaselineWorker(QThread):
    """Run the original Gate 0 ordering with modular execution services."""

    output_ready = Signal(str)
    stage_ready = Signal(str)
    progress_ready = Signal(int, int, str)
    case_ready = Signal(str, str, str, float, str)
    suite_progress_ready = Signal(int, int)
    completed = Signal(object, bool, str, str)

    def __init__(
        self,
        run_id: str,
        plan: BaselinePlan,
        variants: Sequence[RunVariant],
        metadata: dict[str, Any],
    ) -> None:
        super().__init__()
        self.run_id = run_id
        self.plan = plan
        self.variants = tuple(variants)
        self.metadata = metadata
        self.controller = ProcessController(self._output)
        self.engine: ExecutionEngine | None = None
        self.store: EvidenceStore | None = None

    def run(self) -> None:
        outcomes: list[CaseOutcome] = []
        preparation: PreparationFailure | None = None
        source_roots = (
            SOURCE_ROOT / "src",
            SOURCE_ROOT / "include",
            SOURCE_ROOT / "lib",
            SOURCE_ROOT / "CMakeLists.txt",
        )
        try:
            self.store = EvidenceStore(self.run_id, self.metadata)
            before = tree_fingerprint(source_roots)
            self.store.event("BASELINE_SOURCE_FINGERPRINT", {"sha256": before})
            self.stage_ready.emit("Preservar NSC de comparación")
            comparison = snapshot_baseline_comparison(
                self.plan.comparison_source,
                self.store.root / "preparation" / "comparison-nsc.exe",
            )
            self.store.event(
                "BASELINE_COMPARISON_SNAPSHOT",
                {
                    "source": str(self.plan.comparison_source),
                    "snapshot": str(comparison),
                },
            )
            execution_config = replace(
                self.plan.compiler_config,
                comparison_nsc=str(comparison),
            )
            commands = tuple(self.plan.preparation_commands)
            total = len(commands) + 5
            completed = 1
            self.progress_ready.emit(
                completed,
                total,
                "NSC de comparación preservado antes del rebuild",
            )
            for label, command, cwd in commands:
                if label == "Instalar compilador en staging":
                    self.stage_ready.emit("Preparar staging del baseline")
                    reset_baseline_staging(self.plan.staging_prefix)
                    completed += 1
                    self.progress_ready.emit(completed, total, "staging fijo limpio")
                if not self._command(label, command, cwd):
                    preparation = self._preparation_failure(label, command)
                    break
                completed += 1
                self.progress_ready.emit(completed, total, label)
            if preparation is None and not self.controller.cancelled.is_set():
                self.stage_ready.emit("Preparar NSLink del baseline")
                staged = stage_baseline_nslink(
                    self.plan.nslink_source, self.plan.staging_prefix
                )
                self._output(f"NSLink preparado: {staged}\n")
                completed += 1
                self.progress_ready.emit(completed, total, "NSLink verificado por SHA-256")
                label, command, cwd = self.plan.bootstrap_command
                if not self._command(label, command, cwd):
                    preparation = self._preparation_failure(label, command)
                else:
                    completed += 1
                    self.progress_ready.emit(completed, total, label)
                built_fingerprint = ""
                if preparation is None and not self.controller.cancelled.is_set():
                    built_fingerprint = tree_fingerprint(
                        (
                            self.plan.staging_prefix / "bin" / "nsc.exe",
                            self.plan.staging_prefix / "bin" / "nslink.exe",
                            self.plan.staging_prefix / "lib" / "NativeSharpCore.lib",
                        )
                    )
            if preparation is None and not self.controller.cancelled.is_set():
                timeout_budgets = load_variant_timeout_budgets(self.variants)
                self.stage_ready.emit("Certificar matriz completa O0–O3")
                self.engine = ExecutionEngine(
                    execution_config,
                    self.store,
                    self.output_ready.emit,
                    self.case_ready.emit,
                    self.suite_progress_ready.emit,
                    timeout_budgets,
                    lambda phase, done, maximum, detail: self.stage_ready.emit(
                        f"{phase} — {done}/{maximum}: {detail}"
                    ),
                )
                outcomes, _cancelled = self.engine.run(self.variants)
                preparation = self.engine.preparation_failure
            cancelled = self.controller.cancelled.is_set() or (
                self.engine is not None and self.engine.process.cancelled.is_set()
            )
            failed = any(item.status in TERMINAL_FAILURES for item in outcomes)
            after = tree_fingerprint(source_roots)
            final_fingerprint = tree_fingerprint(
                (
                    self.plan.staging_prefix / "bin" / "nsc.exe",
                    self.plan.staging_prefix / "bin" / "nslink.exe",
                    self.plan.staging_prefix / "lib" / "NativeSharpCore.lib",
                )
            )
            detail = "Baseline no promovido"
            stable = before == after and built_fingerprint == final_fingerprint
            if not cancelled and preparation is None and not failed and stable:
                promotion = promote_certified_baseline(
                    self.plan.staging_prefix, self.plan.promotion_prefix
                )
                detail = (
                    "Gate 0 certificado y promovido: "
                    f"{promotion['source']} -> {promotion['destination']}"
                )
                self.store.event("BASELINE_PROMOTED", promotion)
            elif not stable:
                preparation = PreparationFailure(
                    "PREPARATION_FAILED",
                    "Certificar árbol inmóvil",
                    "el árbol fuente o los binarios certificados cambiaron durante la corrida",
                    str(self.store.log_path),
                )
            self.store.finish(outcomes, cancelled, preparation)
            self.completed.emit(outcomes, cancelled, str(self.store.root), detail)
        except Exception as error:
            if self.store is not None:
                self.store.close()
            self.completed.emit(
                outcomes,
                self.controller.cancelled.is_set(),
                f"ERROR: {type(error).__name__}: {error}",
                "Baseline fallido",
            )

    def cancel(self) -> None:
        self.controller.cancel()
        if self.engine is not None:
            self.engine.cancel()

    def _command(self, label: str, command: Sequence[str], cwd: Path) -> bool:
        self.stage_ready.emit(label)
        self._output(f"$ {subprocess.list2cmdline(list(command))}\n")
        code, _captured = self.controller.run(command, cwd)
        return code == 0 and not self.controller.cancelled.is_set()

    def _preparation_failure(
        self, label: str, command: Sequence[str]
    ) -> PreparationFailure:
        assert self.store is not None
        return PreparationFailure(
            "CANCELLED" if self.controller.cancelled.is_set() else "PREPARATION_FAILED",
            label,
            "la etapa no terminó correctamente",
            str(self.store.log_path),
            tuple(command),
        )

    def _output(self, line: str) -> None:
        if self.store is not None:
            self.store.log(line)
        self.output_ready.emit(line)


class BaselinePageMixin:
    def _build_baseline_tab(self) -> QWidget:
        """Build the independent Gate 0 certification and promotion surface."""
        page = QWidget()
        layout = QVBoxLayout(page)
        notice = QLabel(
            "Certifica Gate 0 en el staging fijo de A:, ejecuta la matriz completa "
            "O0–O3 y sólo promueve a C:\\native-sharp cuando todas las etapas y casos "
            "terminan correctamente y el árbol permanece inmóvil."
        )
        notice.setWordWrap(True)
        layout.addWidget(notice)
        paths = QGroupBox("Contrato fijo de baseline")
        form = QFormLayout(paths)
        self.baseline_staging_value = QLineEdit(str(BASELINE_PREFIX.resolve()))
        self.baseline_staging_value.setReadOnly(True)
        self.baseline_destination_value = QLineEdit(
            str(BASELINE_PROMOTION_PREFIX.resolve())
        )
        self.baseline_destination_value.setReadOnly(True)
        self.baseline_profiles_value = QLineEdit(
            ", ".join(BASELINE_OPTIMIZATIONS)
        )
        self.baseline_profiles_value.setReadOnly(True)
        form.addRow("Staging certificado", self.baseline_staging_value)
        form.addRow("Promoción tras PASS", self.baseline_destination_value)
        form.addRow("Matriz obligatoria", self.baseline_profiles_value)
        layout.addWidget(paths)
        controls = QHBoxLayout()
        self.baseline_preview_button = QPushButton("Previsualizar certificación")
        self.baseline_run_button = QPushButton("Certificar / crear baseline")
        self.baseline_cancel_button = QPushButton("Cancelar certificación")
        self.baseline_cancel_button.setEnabled(False)
        controls.addWidget(self.baseline_preview_button)
        controls.addWidget(self.baseline_run_button)
        controls.addWidget(self.baseline_cancel_button)
        controls.addStretch(1)
        layout.addLayout(controls)
        self.baseline_stage = QLabel("Sin certificación activa")
        self.baseline_stage.setWordWrap(True)
        layout.addWidget(self.baseline_stage)
        self.baseline_progress = QProgressBar()
        self.baseline_progress.setRange(0, 1)
        self.baseline_progress.setValue(0)
        layout.addWidget(self.baseline_progress)
        self.baseline_category_summary = QLabel(
            "Sin matriz previsualizada: categorías unitaria y externa pendientes"
        )
        self.baseline_category_summary.setWordWrap(True)
        layout.addWidget(self.baseline_category_summary)
        self.baseline_table = QTableWidget(0, 8)
        self.baseline_table.setObjectName("baseline_test_table")
        self.baseline_table.setHorizontalHeaderLabels(
            (
                "Categoría",
                "Caso",
                "Componente",
                "Build",
                "Opt",
                "Estado",
                "Duración",
                "Detalle / evidencia",
            )
        )
        self.baseline_table.setAlternatingRowColors(True)
        self.baseline_table.setSelectionBehavior(
            QTableWidget.SelectionBehavior.SelectRows
        )
        self.baseline_table.setSelectionMode(
            QAbstractItemView.SelectionMode.SingleSelection
        )
        self.baseline_table.setEditTriggers(
            QTableWidget.EditTrigger.NoEditTriggers
        )
        self.baseline_table.horizontalHeader().setSectionResizeMode(
            QHeaderView.ResizeMode.Interactive
        )
        self.baseline_table.horizontalHeader().setStretchLastSection(True)
        layout.addWidget(self.baseline_table, 2)
        self.baseline_log = QPlainTextEdit()
        self.baseline_log.setReadOnly(True)
        layout.addWidget(self.baseline_log, 1)
        self.baseline_preview_button.clicked.connect(self._preview_baseline)
        self.baseline_run_button.clicked.connect(self._start_baseline)
        self.baseline_cancel_button.clicked.connect(self._cancel_active)
        return page

    def _preview_baseline(
        self, _checked: bool = False, *, show_errors: bool = True
    ) -> None:
        try:
            self.catalog = CanonicalCatalog.load()
            config = self._config_from_ui().with_discovery().validated(
                require_tools=False
            )
            plan = create_baseline_plan(config)
            variants = expand_variants(self.catalog.cases, "Todos", "Todas")
            self._populate_baseline_plan(variants)
            self.baseline_stage.setText(
                f"Plan listo: {len(plan.preparation_commands) + 5} etapas previas/de cierre · "
                f"staging {plan.staging_prefix} · promoción {plan.promotion_prefix}"
            )
        except Exception as error:
            self.baseline_stage.setText(f"Baseline no disponible: {error}")
            if show_errors:
                self._show_error("No se pudo preparar el baseline", error)

    def _start_baseline(self) -> None:
        if self.worker is not None:
            return
        try:
            self.catalog = CanonicalCatalog.load()
            config = self._config_from_ui().with_discovery().validated(
                require_tools=False
            )
            plan = create_baseline_plan(config)
            variants = expand_variants(self.catalog.cases, "Todos", "Todas")
            if not variants:
                raise ValueError("el catálogo no contiene variantes para certificar")
            self._populate_baseline_plan(variants)
            answer = QMessageBox.question(
                self,
                "Certificar baseline",
                "Se reconstruirá y certificará el staging:\n"
                f"{plan.staging_prefix}\n\n"
                "Sólo si toda la matriz pasa se promoverá a:\n"
                f"{plan.promotion_prefix}\n\n¿Continuar?",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
                QMessageBox.StandardButton.No,
            )
            if answer != QMessageBox.StandardButton.Yes:
                return
            run_id = make_run_id()
            metadata = {
                "schema": 1,
                "kind": "baseline-certification",
                "run_id": run_id,
                "revision": current_revision(),
                "variant_count": len(variants),
                "optimizations": list(plan.optimizations),
                "staging_prefix": str(plan.staging_prefix),
                "promotion_prefix": str(plan.promotion_prefix),
            }
            self.baseline_log.clear()
            self.active_release_operation = "baseline"
            self.baseline_progress.setRange(0, len(plan.preparation_commands) + 5)
            self.baseline_progress.setValue(0)
            self._set_busy(True)
            worker = BaselineWorker(run_id, plan, variants, metadata)
            self.worker = worker
            worker.output_ready.connect(self._append_baseline_output)
            worker.stage_ready.connect(self.baseline_stage.setText)
            worker.progress_ready.connect(self._update_baseline_progress)
            worker.case_ready.connect(self._update_baseline_case)
            worker.suite_progress_ready.connect(self._update_baseline_suite_progress)
            worker.completed.connect(self._baseline_finished)
            worker.start()
        except Exception as error:
            self._show_error("No se pudo iniciar la certificación", error)

    def _append_baseline_output(self, text: str) -> None:
        self.baseline_log.moveCursor(self.baseline_log.textCursor().MoveOperation.End)
        self.baseline_log.insertPlainText(text)
        self.baseline_log.ensureCursorVisible()

    def _populate_baseline_plan(
        self,
        variants: Sequence[RunVariant],
        outcomes: Sequence[CaseOutcome] = (),
    ) -> None:
        outcome_by_key = {item.key: item for item in outcomes}
        self.baseline_variants_by_key = {
            variant.key: variant for variant in variants
        }
        self.baseline_status_items: dict[str, QTableWidgetItem] = {}
        case_ids: dict[str, set[str]] = defaultdict(set)
        variant_counts: dict[str, int] = defaultdict(int)
        self.baseline_table.setSortingEnabled(False)
        self.baseline_table.clearContents()
        self.baseline_table.setRowCount(len(variants))
        for row, variant in enumerate(variants):
            category = baseline_test_category(variant.case.layer)
            case_ids[category].add(variant.case.case_id)
            variant_counts[category] += 1
            outcome = outcome_by_key.get(variant.key)
            status = outcome.status if outcome is not None else "LISTO"
            duration = outcome.duration_seconds if outcome is not None else 0.0
            detail = outcome.detail if outcome is not None else ""
            evidence = outcome.evidence if outcome is not None else ""
            merged = (
                f"{detail} | {evidence}"
                if detail and evidence
                else detail or evidence
            )
            values = (
                category,
                variant.case.case_id,
                variant.case.component,
                variant.build_mode,
                variant.optimization,
                status,
                f"{duration:.1f} s" if duration else "",
                merged,
            )
            for column, value in enumerate(values):
                item = QTableWidgetItem(value)
                if column == 1:
                    item.setData(Qt.ItemDataRole.UserRole, variant.key)
                if column == 5:
                    self._color_status(item, status)
                    self.baseline_status_items[variant.key] = item
                self.baseline_table.setItem(row, column, item)
        self.baseline_table.setSortingEnabled(True)
        self.baseline_category_summary.setText(
            f"{BASELINE_UNIT_CATEGORY}: "
            f"{len(case_ids[BASELINE_UNIT_CATEGORY]):,} casos / "
            f"{variant_counts[BASELINE_UNIT_CATEGORY]:,} variantes · "
            f"{BASELINE_EXTERNAL_CATEGORY}: "
            f"{len(case_ids[BASELINE_EXTERNAL_CATEGORY]):,} casos / "
            f"{variant_counts[BASELINE_EXTERNAL_CATEGORY]:,} variantes"
        )

    def _update_baseline_case(
        self,
        key: str,
        status: str,
        detail: str,
        duration: float,
        evidence: str,
    ) -> None:
        item = self.baseline_status_items.get(key)
        if item is None:
            return
        sorting_enabled = self.baseline_table.isSortingEnabled()
        self.baseline_table.setSortingEnabled(False)
        row = item.row()
        status_item = QTableWidgetItem(status)
        self._color_status(status_item, status)
        self.baseline_table.setItem(row, 5, status_item)
        self.baseline_status_items[key] = status_item
        self.baseline_table.setItem(
            row, 6, QTableWidgetItem(f"{duration:.1f} s" if duration else "")
        )
        merged = (
            f"{detail} | {evidence}"
            if detail and evidence
            else detail or evidence
        )
        self.baseline_table.setItem(row, 7, QTableWidgetItem(merged))
        self.baseline_table.setSortingEnabled(sorting_enabled)
        self.baseline_stage.setText(f"{status}: {key} — {merged or 'sin detalle'}")

    def _update_baseline_progress(
        self, current: int, total: int, detail: str
    ) -> None:
        self.baseline_progress.setRange(0, max(1, total))
        self.baseline_progress.setValue(current)
        self.baseline_progress.setFormat(f"{current}/{total} etapas (%p%)")
        self.baseline_stage.setText(detail)

    def _update_baseline_suite_progress(self, current: int, total: int) -> None:
        self.baseline_progress.setRange(0, max(1, total))
        self.baseline_progress.setValue(current)
        self.baseline_progress.setFormat(f"{current:,}/{total:,} variantes (%p%)")

    def _baseline_finished(
        self, outcomes: object, cancelled: bool, evidence: str, detail: str
    ) -> None:
        typed = list(outcomes) if isinstance(outcomes, list) else []
        failed = evidence.startswith("ERROR:") or any(
            item.status in TERMINAL_FAILURES for item in typed
        )
        self.baseline_stage.setText(
            f"{detail} · evidencia: {evidence}"
            if not evidence.startswith("ERROR:")
            else evidence
        )
        self.baseline_progress.setValue(
            0 if failed or cancelled else self.baseline_progress.maximum()
        )
        if typed:
            self._populate_baseline_plan(
                tuple(self.baseline_variants_by_key.values()), typed
            )
        self.worker = None
        self.active_release_operation = ""
        self._set_busy(False)
        self._refresh_history()


__all__ = ("BaselinePageMixin", "BaselineWorker")
