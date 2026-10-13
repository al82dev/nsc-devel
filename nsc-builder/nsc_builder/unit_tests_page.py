"""Independent canonical unit-test workspace for NSC Builder 3."""

from __future__ import annotations

from typing import Sequence

from PySide6.QtCore import Qt
from PySide6.QtWidgets import (
    QAbstractItemView,
    QComboBox,
    QGridLayout,
    QHeaderView,
    QLabel,
    QProgressBar,
    QPushButton,
    QTableWidget,
    QTableWidgetItem,
    QVBoxLayout,
    QWidget,
)

from .catalog import expand_variants, refresh_run_variants
from .configuration import BuilderConfig
from .execution import retry_variants
from .models import CaseOutcome, CaseRecord, PreparationFailure, RunVariant
from .runtime_support import make_run_id
from .unit_tests import select_unit_cases
from .utils import current_revision


class UnitTestsPageMixin:
    def _build_unit_tests_tab(self) -> QWidget:
        """Build an independent projection of canonical unit-layer tests."""
        page = QWidget()
        layout = QVBoxLayout(page)
        notice = QLabel(
            "Sólo muestra casos canónicos cuya asignación declara layer=unit. "
            "Usa el mismo modelo, preparación de productores, evidencia y cancelación "
            "que el ejecutor principal, sin duplicar definiciones."
        )
        notice.setWordWrap(True)
        layout.addWidget(notice)
        controls = QGridLayout()
        self.unit_case_combo = QComboBox()
        self.unit_case_combo.setEditable(True)
        self.unit_profile_combo = QComboBox()
        self.unit_profile_combo.addItems(["Todos", "PR", "NIGHTLY", "WEEKLY"])
        self.unit_execution_mode = QComboBox()
        self.unit_execution_mode.addItems(["Debug", "Release", "Todos"])
        self.unit_optimization_combo = QComboBox()
        self.unit_optimization_combo.addItems(["O0", "O1", "O2", "O3", "Todas"])
        self.unit_preview_button = QPushButton("Previsualizar unitarias")
        self.unit_run_button = QPushButton("Ejecutar unitarias")
        self.unit_retry_button = QPushButton("Reejecutar fallos unitarios")
        self.unit_cancel_button = QPushButton("Cancelar corrida/árbol")
        self.unit_retry_button.setEnabled(False)
        self.unit_cancel_button.setEnabled(False)
        controls.addWidget(QLabel("Caso (vacío = todos)"), 0, 0)
        controls.addWidget(self.unit_case_combo, 0, 1)
        controls.addWidget(QLabel("Perfil"), 0, 2)
        controls.addWidget(self.unit_profile_combo, 0, 3)
        controls.addWidget(QLabel("Build"), 1, 0)
        controls.addWidget(self.unit_execution_mode, 1, 1)
        controls.addWidget(QLabel("Optimización"), 1, 2)
        controls.addWidget(self.unit_optimization_combo, 1, 3)
        controls.addWidget(self.unit_preview_button, 0, 4)
        controls.addWidget(self.unit_run_button, 0, 5)
        controls.addWidget(self.unit_retry_button, 1, 4)
        controls.addWidget(self.unit_cancel_button, 1, 5)
        layout.addLayout(controls)
        self.unit_plan_label = QLabel()
        unit_count = sum(case.layer == "unit" for case in self.catalog.cases)
        self.unit_plan_label.setText(
            f"Capa unitaria: {unit_count:,} casos canónicos; pulsa Previsualizar "
            "para expandir la configuración elegida."
        )
        self.unit_plan_label.setWordWrap(True)
        layout.addWidget(self.unit_plan_label)
        self.unit_activity = QLabel("Preparada; ninguna prueba unitaria ha iniciado")
        self.unit_activity.setWordWrap(True)
        layout.addWidget(self.unit_activity)
        self.unit_progress = QProgressBar()
        self.unit_progress.setRange(0, 1)
        self.unit_progress.setValue(0)
        self.unit_progress.setFormat("0/0 pruebas unitarias")
        layout.addWidget(self.unit_progress)
        self.unit_table = QTableWidget(0, 10)
        self.unit_table.setHorizontalHeaderLabels(
            (
                "Caso",
                "Contrato",
                "Componente",
                "Build",
                "Opt",
                "Perfil",
                "Ejecutor",
                "Estado",
                "Duración",
                "Detalle / evidencia",
            )
        )
        self.unit_table.setAlternatingRowColors(True)
        self.unit_table.setSelectionBehavior(
            QTableWidget.SelectionBehavior.SelectRows
        )
        self.unit_table.setSelectionMode(
            QAbstractItemView.SelectionMode.SingleSelection
        )
        self.unit_table.setEditTriggers(QTableWidget.EditTrigger.NoEditTriggers)
        self.unit_table.horizontalHeader().setSectionResizeMode(
            QHeaderView.ResizeMode.Interactive
        )
        self.unit_table.horizontalHeader().setStretchLastSection(True)
        layout.addWidget(self.unit_table, 1)
        self.unit_preview_button.clicked.connect(self._preview_unit_plan)
        self.unit_run_button.clicked.connect(self._start_unit_suite)
        self.unit_retry_button.clicked.connect(self._retry_unit_failures)
        self.unit_cancel_button.clicked.connect(self._cancel_active)
        return page

    def _unit_cases(self) -> tuple[CaseRecord, ...]:
        """Select only canonical unit-layer rows without duplicating definitions."""
        requested = self.unit_case_combo.currentText().strip()
        return select_unit_cases(
            self.catalog.cases,
            requested,
            self.unit_profile_combo.currentText(),
        )

    def _preview_unit_plan(self) -> None:
        try:
            cases = self._unit_cases()
            variants = expand_variants(
                cases,
                self.unit_execution_mode.currentText(),
                self.unit_optimization_combo.currentText(),
            )
            self._populate_unit_plan(variants)
        except Exception as error:
            self._show_error("No se pudo preparar la selección unitaria", error)

    def _populate_unit_plan(
        self,
        variants: Sequence[RunVariant],
        outcomes: Sequence[CaseOutcome] = (),
    ) -> None:
        outcome_by_key = {item.key: item for item in outcomes}
        self.unit_variants_by_key = {variant.key: variant for variant in variants}
        self.unit_status_items.clear()
        self.unit_table.setSortingEnabled(False)
        self.unit_table.clearContents()
        self.unit_table.setRowCount(len(variants))
        for row, variant in enumerate(variants):
            outcome = outcome_by_key.get(variant.key)
            status = outcome.status if outcome is not None else "LISTO"
            detail = outcome.detail if outcome is not None else ""
            duration = outcome.duration_seconds if outcome is not None else 0.0
            values = (
                variant.case.case_id,
                variant.case.contract_id,
                variant.case.component,
                variant.build_mode,
                variant.optimization,
                variant.case.profile,
                variant.case.executor,
                status,
                f"{duration:.1f} s" if duration else "",
                detail,
            )
            for column, value in enumerate(values):
                item = QTableWidgetItem(value)
                if column == 0:
                    item.setData(Qt.ItemDataRole.UserRole, variant.key)
                if column == 7:
                    self._color_status(item, status)
                    self.unit_status_items[variant.key] = item
                self.unit_table.setItem(row, column, item)
        self.unit_table.setSortingEnabled(True)
        case_count = len({variant.case.case_id for variant in variants})
        self.unit_plan_label.setText(
            f"Capa unitaria: {case_count:,} casos canónicos · "
            f"{len(variants):,} variantes ejecutables"
        )
        self.unit_progress.setRange(0, max(1, len(variants)))
        self.unit_progress.setValue(0)
        self.unit_progress.setFormat(f"0/{len(variants):,} pruebas unitarias")
        self.unit_activity.setText("Preparada; ninguna prueba unitaria ha iniciado")

    def _start_unit_suite(self) -> None:
        if self.worker is not None:
            return
        try:
            self.catalog = CanonicalCatalog.load()
            config = self._config_from_ui().with_discovery().validated(
                require_tools=False
            )
            self._load_config(config)
            config.save()
            cases = self._unit_cases()
            variants = expand_variants(
                cases,
                self.unit_execution_mode.currentText(),
                self.unit_optimization_combo.currentText(),
            )
            if not variants:
                raise ValueError("la selección unitaria no contiene variantes ejecutables")
            self._populate_unit_plan(variants)
            self._run_unit_variants(config, variants, "unit-selection")
        except Exception as error:
            self._show_error("No se pudo iniciar la ejecución unitaria", error)

    def _run_unit_variants(
        self,
        config: BuilderConfig,
        variants: Sequence[RunVariant],
        origin: str,
    ) -> None:
        variants = refresh_run_variants(self.catalog, variants)
        run_id = make_run_id()
        metadata = {
            "schema": 1,
            "kind": "phase-2-unit-suite",
            "origin": origin,
            "run_id": run_id,
            "revision": current_revision(),
            "variant_count": len(variants),
            "case_count": len({variant.case.case_id for variant in variants}),
            "selection": {
                "layer": "unit",
                "case": self.unit_case_combo.currentText().strip(),
                "profile": self.unit_profile_combo.currentText(),
                "build_mode": self.unit_execution_mode.currentText(),
                "optimization": self.unit_optimization_combo.currentText(),
            },
        }
        self.active_suite_surface = "unit"
        self.full_log.clear()
        self.error_report.clear()
        self.unit_activity.setText("Preparar pruebas unitarias — planificando targets")
        self._set_busy(True)
        worker = self._suite_worker_type(run_id, config, variants, metadata)
        self.worker = worker
        worker.output_ready.connect(self._append_full_output)
        worker.case_ready.connect(self._update_unit_case)
        worker.progress_ready.connect(self._update_unit_progress)
        worker.preparation_ready.connect(self._update_unit_preparation)
        worker.completed.connect(self._unit_suite_finished)
        worker.finished.connect(self._release_finished_suite_worker)
        worker.start()

    def _update_unit_case(
        self,
        key: str,
        status: str,
        detail: str,
        duration: float,
        evidence: str,
    ) -> None:
        item = self.unit_status_items.get(key)
        if item is None:
            return
        row = item.row()
        status_item = QTableWidgetItem(status)
        self._color_status(status_item, status)
        self.unit_table.setItem(row, 7, status_item)
        self.unit_status_items[key] = status_item
        self.unit_table.setItem(
            row, 8, QTableWidgetItem(f"{duration:.1f} s" if duration else "")
        )
        merged = f"{detail} | {evidence}" if detail and evidence else detail or evidence
        self.unit_table.setItem(row, 9, QTableWidgetItem(merged))
        self.unit_activity.setText(f"{status}: {key} — {merged or 'sin salida todavía'}")

    def _update_unit_progress(self, done: int, total: int) -> None:
        self.unit_progress.setRange(0, max(1, total))
        self.unit_progress.setValue(done)
        self.unit_progress.setFormat(f"{done:,}/{total:,} pruebas unitarias (%p%)")

    def _update_unit_preparation(
        self, phase: str, done: int, total: int, detail: str
    ) -> None:
        self.unit_progress.setRange(0, max(1, total))
        self.unit_progress.setValue(done)
        self.unit_progress.setFormat(f"{phase}: {done:,}/{total:,} targets (%p%)")
        self.unit_activity.setText(f"{phase} — {detail}")

    def _unit_suite_finished(
        self,
        outcomes: object,
        cancelled: bool,
        evidence: str,
        preparation: object,
    ) -> None:
        typed = list(outcomes) if isinstance(outcomes, list) else []
        variants = tuple(self.unit_variants_by_key.values())
        self.unit_last_failed_variants = retry_variants(variants, typed)
        prepared_failure = (
            preparation if isinstance(preparation, PreparationFailure) else None
        )
        self._render_error_report(typed, cancelled, evidence, prepared_failure)
        self.unit_progress.setRange(0, max(1, len(variants)))
        self.unit_progress.setValue(len(typed) if prepared_failure is None else 0)
        self.unit_progress.setFormat(
            "PREPARACIÓN FALLIDA"
            if prepared_failure is not None
            else f"{len(typed):,}/{len(variants):,} pruebas unitarias — TERMINADO"
        )
        self.unit_activity.setText(
            f"Ejecución unitaria {'cancelada' if cancelled else 'terminada'} · "
            f"evidencia: {evidence}"
        )
        self.worker = None
        self._set_busy(False)
        self._refresh_history()
        if typed:
            self._populate_unit_plan(variants, typed)

    def _retry_unit_failures(self) -> None:
        if self.worker is not None or not self.unit_last_failed_variants:
            return
        try:
            config = self._config_from_ui().with_discovery().validated(
                require_tools=False
            )
            self._populate_unit_plan(self.unit_last_failed_variants)
            self._run_unit_variants(
                config, self.unit_last_failed_variants, "unit-rerun-failures"
            )
        except Exception as error:
            self._show_error("No se pudieron reejecutar los fallos unitarios", error)


__all__ = ("UnitTestsPageMixin",)
