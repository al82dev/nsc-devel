"""Independent local deployment workspace for NSC Builder 3."""

from __future__ import annotations

from PySide6.QtWidgets import (
    QComboBox,
    QFormLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QMessageBox,
    QPlainTextEdit,
    QProgressBar,
    QPushButton,
    QVBoxLayout,
    QWidget,
)

from .build import compiler_build_dir
from .constants import TERMINAL_FAILURES
from .deploy import DEPLOY_PREFIX, create_deploy_plan
from .runtime_support import make_run_id
from .utils import current_revision


class DeployPageMixin:
    def _build_deploy_tab(self) -> QWidget:
        """Build the independent local C-drive installation surface."""
        page = QWidget()
        layout = QVBoxLayout(page)
        notice = QLabel(
            "Instala un build de NSC ya configurado en C:\\native-sharp para pruebas "
            "locales. Este flujo no certifica, crea ni consume un baseline."
        )
        notice.setWordWrap(True)
        layout.addWidget(notice)
        form_group = QGroupBox("Instalación para pruebas")
        form = QFormLayout(form_group)
        self.deploy_mode = QComboBox()
        self.deploy_mode.addItems(["Release", "Debug"])
        self.deploy_build_value = QLineEdit()
        self.deploy_build_value.setReadOnly(True)
        self.deploy_destination_value = QLineEdit(str(DEPLOY_PREFIX.resolve()))
        self.deploy_destination_value.setReadOnly(True)
        form.addRow("Configuración", self.deploy_mode)
        form.addRow("Build de CMake", self.deploy_build_value)
        form.addRow("Destino fijo", self.deploy_destination_value)
        layout.addWidget(form_group)
        controls = QHBoxLayout()
        self.deploy_preview_button = QPushButton("Previsualizar instalación")
        self.deploy_run_button = QPushButton("Instalar para pruebas")
        self.deploy_cancel_button = QPushButton("Cancelar instalación")
        self.deploy_cancel_button.setEnabled(False)
        controls.addWidget(self.deploy_preview_button)
        controls.addWidget(self.deploy_run_button)
        controls.addWidget(self.deploy_cancel_button)
        controls.addStretch(1)
        layout.addLayout(controls)
        self.deploy_stage = QLabel("Sin instalación activa")
        self.deploy_stage.setWordWrap(True)
        layout.addWidget(self.deploy_stage)
        self.deploy_progress = QProgressBar()
        self.deploy_progress.setRange(0, 2)
        self.deploy_progress.setValue(0)
        layout.addWidget(self.deploy_progress)
        self.deploy_log = QPlainTextEdit()
        self.deploy_log.setReadOnly(True)
        layout.addWidget(self.deploy_log, 1)
        self.deploy_mode.currentTextChanged.connect(self._preview_deploy)
        self.deploy_preview_button.clicked.connect(self._preview_deploy)
        self.deploy_run_button.clicked.connect(self._start_deploy)
        self.deploy_cancel_button.clicked.connect(self._cancel_active)
        return page

    def _preview_deploy(self, _checked: bool = False, *, show_errors: bool = True) -> None:
        try:
            config = self._config_from_ui().with_discovery().validated(
                require_tools=False
            )
            plan = create_deploy_plan(config, self.deploy_mode.currentText())
            self.deploy_build_value.setText(str(plan.build_dir))
            self.deploy_stage.setText(
                f"Plan listo: {plan.build_dir} -> {plan.prefix}; "
                "verificación final con nsc --version"
            )
        except Exception as error:
            config = self._config_from_ui()
            self.deploy_build_value.setText(
                str(compiler_build_dir(config, self.deploy_mode.currentText()))
            )
            self.deploy_stage.setText(f"Deploy no disponible: {error}")
            if show_errors:
                self._show_error("No se pudo preparar el deploy", error)

    def _start_deploy(self) -> None:
        if self.worker is not None:
            return
        try:
            config = self._config_from_ui().with_discovery().validated(
                require_tools=False
            )
            plan = create_deploy_plan(config, self.deploy_mode.currentText())
            answer = QMessageBox.question(
                self,
                "Instalar para pruebas",
                f"Se instalará el build:\n{plan.build_dir}\n\nen:\n"
                f"{plan.prefix}\n\nEste flujo no certifica baseline. ¿Continuar?",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
                QMessageBox.StandardButton.No,
            )
            if answer != QMessageBox.StandardButton.Yes:
                return
            run_id = make_run_id()
            metadata = {
                "schema": 1,
                "kind": "deploy-for-testing",
                "run_id": run_id,
                "revision": current_revision(),
                "build_mode": plan.build_mode,
                "build_dir": str(plan.build_dir),
                "prefix": str(plan.prefix),
                "commands": [
                    {"label": label, "command": command, "cwd": str(cwd)}
                    for label, command, cwd in plan.commands
                ],
            }
            self.deploy_log.clear()
            self.active_release_operation = "deploy"
            self.deploy_progress.setRange(0, len(plan.commands))
            self.deploy_progress.setValue(0)
            self._set_busy(True)
            worker = self._command_worker_type(run_id, plan.commands, metadata)
            self.worker = worker
            worker.output_ready.connect(self._append_deploy_output)
            worker.stage_ready.connect(self._set_deploy_stage)
            worker.completed.connect(self._deploy_finished)
            worker.start()
        except Exception as error:
            self._show_error("No se pudo iniciar el deploy", error)

    def _append_deploy_output(self, text: str) -> None:
        self.deploy_log.moveCursor(self.deploy_log.textCursor().MoveOperation.End)
        self.deploy_log.insertPlainText(text)
        self.deploy_log.ensureCursorVisible()

    def _set_deploy_stage(self, stage: str) -> None:
        self.deploy_stage.setText(stage)
        self.deploy_progress.setValue(min(self.deploy_progress.value() + 1, 2))

    def _deploy_finished(
        self, outcomes: object, cancelled: bool, evidence: str
    ) -> None:
        typed = list(outcomes) if isinstance(outcomes, list) else []
        failed = evidence.startswith("ERROR:") or any(
            item.status in TERMINAL_FAILURES for item in typed
        )
        if not failed and not cancelled:
            self.deploy_progress.setValue(self.deploy_progress.maximum())
            self.deploy_stage.setText(
                f"Deploy verificado en {DEPLOY_PREFIX.resolve()} · evidencia: {evidence}"
            )
        else:
            self.deploy_stage.setText(
                f"Deploy {'cancelado' if cancelled else 'fallido'} · evidencia: {evidence}"
            )
        self.worker = None
        self.active_release_operation = ""
        self._set_busy(False)
        self._refresh_history()


__all__ = ("DeployPageMixin",)
