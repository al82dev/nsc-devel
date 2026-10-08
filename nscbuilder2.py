#!/usr/bin/env python3
"""Independent PySide6 orchestrator for the consolidated NSC test graph."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from _nscbuilder2 import (
    BuilderConfig,
    BuilderStorage,
    PipelineRunner,
    RunSelection,
    create_run_plan,
)


SOURCE_ROOT = Path(__file__).resolve().parent
STORAGE_ROOT = SOURCE_ROOT / "_nscbuilder2"


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="NSC Builder 2")
    parser.add_argument("--headless-plan", action="store_true")
    parser.add_argument("--headless-run", action="store_true")
    parser.add_argument("--gui-smoke", action="store_true")
    parser.add_argument("--source-root", type=Path)
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--nslink", type=Path)
    parser.add_argument("--suite-prefix", type=Path)
    parser.add_argument("--jobs", type=int)
    parser.add_argument("--unit", action="store_true")
    parser.add_argument("--cases", action="store_true")
    parser.add_argument("--specialized", action="store_true")
    selector = parser.add_mutually_exclusive_group()
    selector.add_argument("--category")
    selector.add_argument("--case")
    return parser


def _configuration(arguments, storage: BuilderStorage) -> BuilderConfig:
    fallback = BuilderConfig.defaults(SOURCE_ROOT)
    stored = storage.load_config(fallback)
    return BuilderConfig(
        source_root=arguments.source_root or stored.source_root,
        build_dir=arguments.build_dir or stored.build_dir,
        nslink=arguments.nslink or stored.nslink,
        suite_prefix=arguments.suite_prefix or stored.suite_prefix,
        jobs=arguments.jobs or stored.jobs,
        build_type=stored.build_type,
    )


def _selection(arguments) -> RunSelection:
    explicit = arguments.unit or arguments.cases or arguments.specialized or arguments.category or arguments.case
    if not explicit:
        return RunSelection(unit=True, manifest_mode="all", specialized=True)
    manifest_mode = "none"
    value = ""
    if arguments.category:
        manifest_mode, value = "category", arguments.category
    elif arguments.case:
        manifest_mode, value = "case", arguments.case
    elif arguments.cases:
        manifest_mode = "all"
    return RunSelection(arguments.unit, manifest_mode, value, arguments.specialized)


def _new_plan(storage: BuilderStorage, config: BuilderConfig, selection: RunSelection):
    run_id, history, work = storage.new_run()
    return create_run_plan(config, selection, run_id, history, work)


def _headless(arguments) -> int:
    storage = BuilderStorage(STORAGE_ROOT)
    config = _configuration(arguments, storage).validated()
    selection = _selection(arguments).validated()
    storage.save_config(config)
    plan = _new_plan(storage, config, selection)
    if arguments.headless_plan:
        print(json.dumps(plan.to_dict(), indent=2, sort_keys=True))
        return 0
    runner = PipelineRunner()
    result = runner.run(
        plan,
        on_output=lambda line: print(line, end="", flush=True),
        on_progress=lambda done, total, title: print(
            f"[Builder 2 {done}/{total}] {title}", flush=True
        ),
    )
    print(json.dumps(result.to_dict(), indent=2, sort_keys=True))
    return result.exit_code


def _gui(arguments, smoke: bool = False) -> int:
    try:
        from PySide6.QtCore import QThread, QTimer, Signal
        from PySide6.QtWidgets import (
            QApplication,
            QCheckBox,
            QComboBox,
            QFileDialog,
            QFormLayout,
            QHBoxLayout,
            QLabel,
            QLineEdit,
            QMainWindow,
            QMessageBox,
            QPlainTextEdit,
            QProgressBar,
            QPushButton,
            QSpinBox,
            QTabWidget,
            QTableWidget,
            QTableWidgetItem,
            QVBoxLayout,
            QWidget,
        )
    except ModuleNotFoundError:
        print("PySide6 is required for the GUI: python -m pip install PySide6", file=sys.stderr)
        return 2

    class RunnerThread(QThread):
        output = Signal(str)
        progress = Signal(int, int, str)
        completed = Signal(object)

        def __init__(self, plan) -> None:
            super().__init__()
            self.runner = PipelineRunner()
            self.plan = plan

        def run(self) -> None:
            result = self.runner.run(
                self.plan,
                on_output=self.output.emit,
                on_progress=self.progress.emit,
            )
            self.completed.emit(result)

        def cancel(self) -> None:
            self.runner.cancel()

    class MainWindow(QMainWindow):
        def __init__(self) -> None:
            super().__init__()
            self.storage = BuilderStorage(STORAGE_ROOT)
            self.thread: RunnerThread | None = None
            config = _configuration(arguments, self.storage)
            self.setWindowTitle("NSC Builder 2 — consolidated O0 tests")
            self.resize(1280, 820)

            root = QWidget()
            layout = QVBoxLayout(root)
            form = QFormLayout()
            self.source = QLineEdit(str(config.source_root))
            self.build = QLineEdit(str(config.build_dir))
            self.nslink = QLineEdit(str(config.nslink))
            self.prefix = QLineEdit(str(config.suite_prefix))
            self.jobs = QSpinBox()
            self.jobs.setRange(1, 128)
            self.jobs.setValue(config.jobs)
            for label, editor, file_selector in (
                ("Source root", self.source, False),
                ("Build directory", self.build, False),
                ("NSLink", self.nslink, True),
                ("Suite prefix", self.prefix, False),
            ):
                row = QHBoxLayout()
                row.addWidget(editor)
                browse = QPushButton("Browse")
                browse.clicked.connect(
                    lambda _checked=False, target=editor, is_file=file_selector: self.browse(
                        target, is_file
                    )
                )
                row.addWidget(browse)
                form.addRow(label, row)
            form.addRow("Parallel jobs", self.jobs)
            layout.addLayout(form)

            choices = QHBoxLayout()
            self.unit = QCheckBox("Consolidated unit registry")
            self.unit.setChecked(True)
            self.manifest_mode = QComboBox()
            self.manifest_mode.addItems(["all", "none", "category", "case"])
            self.manifest_value = QLineEdit()
            self.manifest_value.setPlaceholderText("category or case ID")
            self.specialized = QCheckBox("Specialized O0 suites")
            self.specialized.setChecked(True)
            choices.addWidget(self.unit)
            choices.addWidget(QLabel("NativeSharp manifests:"))
            choices.addWidget(self.manifest_mode)
            choices.addWidget(self.manifest_value)
            choices.addWidget(self.specialized)
            layout.addLayout(choices)

            buttons = QHBoxLayout()
            self.start_button = QPushButton("Configure, build and run")
            self.cancel_button = QPushButton("Cancel descendants")
            self.cancel_button.setEnabled(False)
            self.start_button.clicked.connect(self.start_run)
            self.cancel_button.clicked.connect(self.cancel_run)
            buttons.addWidget(self.start_button)
            buttons.addWidget(self.cancel_button)
            layout.addLayout(buttons)

            self.progress = QProgressBar()
            self.status = QLabel("Ready")
            layout.addWidget(self.progress)
            layout.addWidget(self.status)

            tabs = QTabWidget()
            self.output = QPlainTextEdit()
            self.output.setReadOnly(True)
            self.errors = QPlainTextEdit()
            self.errors.setReadOnly(True)
            self.history = QTableWidget(0, 5)
            self.history.setHorizontalHeaderLabels(["Run", "Status", "Duration", "Compiler", "History"])
            tabs.addTab(self.output, "Full output")
            tabs.addTab(self.errors, "Causal errors")
            tabs.addTab(self.history, "History")
            layout.addWidget(tabs)
            self.setCentralWidget(root)
            self.reload_history()

        def browse(self, editor: QLineEdit, file_selector: bool) -> None:
            if file_selector:
                selected, _filter = QFileDialog.getOpenFileName(
                    self, "Select executable", editor.text(), "Executables (*.exe);;All files (*)"
                )
            else:
                selected = QFileDialog.getExistingDirectory(
                    self, "Select directory", editor.text()
                )
            if selected:
                editor.setText(selected)

        def current_config(self) -> BuilderConfig:
            return BuilderConfig(
                Path(self.source.text()),
                Path(self.build.text()),
                Path(self.nslink.text()),
                Path(self.prefix.text()),
                self.jobs.value(),
            ).validated()

        def current_selection(self) -> RunSelection:
            return RunSelection(
                self.unit.isChecked(),
                self.manifest_mode.currentText(),
                self.manifest_value.text(),
                self.specialized.isChecked(),
            ).validated()

        def start_run(self) -> None:
            try:
                config = self.current_config()
                selection = self.current_selection()
                self.storage.save_config(config)
                plan = _new_plan(self.storage, config, selection)
            except Exception as error:
                QMessageBox.critical(self, "Builder 2 configuration", str(error))
                return
            self.output.clear()
            self.errors.clear()
            self.progress.setMaximum(len(plan.steps))
            self.progress.setValue(0)
            self.start_button.setEnabled(False)
            self.cancel_button.setEnabled(True)
            self.thread = RunnerThread(plan)
            self.thread.output.connect(self.output.insertPlainText)
            self.thread.progress.connect(self.on_progress)
            self.thread.completed.connect(self.on_completed)
            self.thread.start()

        def cancel_run(self) -> None:
            if self.thread is not None:
                self.thread.cancel()
                self.status.setText("Cancelling current process tree...")

        def on_progress(self, done: int, total: int, title: str) -> None:
            self.progress.setMaximum(total)
            self.progress.setValue(done)
            self.status.setText(title)

        def on_completed(self, result) -> None:
            self.start_button.setEnabled(True)
            self.cancel_button.setEnabled(False)
            self.status.setText(f"{result.status}: exit {result.exit_code}")
            self.errors.setPlainText("\n".join(result.errors) or "No causal errors recorded.")
            self.reload_history()
            self.thread = None

        def reload_history(self) -> None:
            rows = self.storage.load_history()
            self.history.setRowCount(len(rows))
            for row_index, row in enumerate(rows):
                values = (
                    row.get("run_id", ""),
                    row.get("status", ""),
                    str(row.get("duration_seconds", "")),
                    row.get("compiler", ""),
                    row.get("history_dir", ""),
                )
                for column, value in enumerate(values):
                    self.history.setItem(row_index, column, QTableWidgetItem(str(value)))

        def closeEvent(self, event) -> None:
            if self.thread is not None and self.thread.isRunning():
                QMessageBox.warning(self, "Builder 2 running", "Cancel the active run before closing.")
                event.ignore()
                return
            try:
                self.storage.save_config(self.current_config())
            except Exception:
                pass
            event.accept()

    application = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    if smoke:
        QTimer.singleShot(0, application.quit)
    return application.exec()


def main(argv: list[str] | None = None) -> int:
    arguments = _parser().parse_args(argv)
    if arguments.headless_plan or arguments.headless_run:
        return _headless(arguments)
    return _gui(arguments, arguments.gui_smoke)


if __name__ == "__main__":
    raise SystemExit(main())
