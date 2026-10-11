"""Instalador gráfico de NativeSharp mediante la instalación oficial de CMake."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys

from PySide6.QtCore import QProcess, QSettings, QTimer, Slot
from PySide6.QtGui import QCloseEvent, QFontDatabase, QTextCursor
from PySide6.QtWidgets import (
    QApplication,
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
    QVBoxLayout,
    QWidget,
)


ROOT = Path(__file__).resolve().parent
DEFAULT_BUILD = ROOT / "build-clang"
DEFAULT_PREFIX = Path(r"C:\native-sharp")


def resolved(path: str | Path) -> Path:
    return Path(path).expanduser().resolve()


def deployment_problems(build: Path, prefix: Path) -> list[str]:
    problems: list[str] = []
    if not (build / "cmake_install.cmake").is_file():
        problems.append(f"El build no contiene cmake_install.cmake: {build}")
    if shutil.which("cmake") is None:
        problems.append("No se encontró cmake.exe en PATH.")
    if prefix == Path(prefix.anchor):
        problems.append("El directorio de instalación no puede ser la raíz de una unidad.")
    if build == prefix:
        problems.append("El build y el directorio de instalación no pueden ser el mismo.")
    return problems


def command_text(arguments: list[str]) -> str:
    return subprocess.list2cmdline(arguments)


class DeployWindow(QMainWindow):
    def __init__(self, build: Path | None = None, prefix: Path | None = None) -> None:
        super().__init__()
        self.setWindowTitle("NativeSharp Deploy")
        self.resize(900, 620)
        self.settings = QSettings("NativeSharp", "Deploy")
        self.process: QProcess | None = None
        self.close_after_cancel = False

        panel = QWidget()
        self.setCentralWidget(panel)
        layout = QVBoxLayout(panel)

        title = QLabel("NativeSharp Deploy")
        title.setStyleSheet("font-size: 22px; font-weight: 600;")
        layout.addWidget(title)
        note = QLabel(
            "Instala un build de CMake mediante cmake --install. El directorio elegido recibirá "
            "bin, lib, share y el resto de la distribución oficial."
        )
        note.setWordWrap(True)
        layout.addWidget(note)

        form = QFormLayout()
        build_value = build if build is not None else self.settings.value("build", str(DEFAULT_BUILD))
        prefix_value = (
            prefix if prefix is not None else self.settings.value("prefix", str(DEFAULT_PREFIX))
        )
        self.build_edit = QLineEdit(str(build_value))
        self.prefix_edit = QLineEdit(str(prefix_value))
        form.addRow("Build de CMake", self._path_row(self.build_edit, self._browse_build))
        form.addRow("Instalar en", self._path_row(self.prefix_edit, self._browse_prefix))
        layout.addLayout(form)

        self.command_label = QLabel()
        self.command_label.setWordWrap(True)
        self.command_label.setStyleSheet(
            "background: #181825; border: 1px solid #45475a; padding: 8px; "
            "font-family: Consolas, monospace;"
        )
        layout.addWidget(self.command_label)

        self.console = QPlainTextEdit()
        self.console.setReadOnly(True)
        self.console.setFont(QFontDatabase.systemFont(QFontDatabase.SystemFont.FixedFont))
        layout.addWidget(self.console, 1)

        self.progress = QProgressBar()
        self.progress.setRange(0, 1)
        self.progress.setValue(0)
        layout.addWidget(self.progress)

        controls = QHBoxLayout()
        self.install_button = QPushButton("Instalar / Deploy")
        self.install_button.clicked.connect(self.start_deploy)
        self.cancel_button = QPushButton("Cancelar")
        self.cancel_button.setEnabled(False)
        self.cancel_button.clicked.connect(self.cancel_deploy)
        self.status = QLabel("Listo")
        controls.addWidget(self.install_button)
        controls.addWidget(self.cancel_button)
        controls.addWidget(self.status, 1)
        layout.addLayout(controls)

        self.build_edit.textChanged.connect(self._update_command)
        self.prefix_edit.textChanged.connect(self._update_command)
        self._update_command()

    def _path_row(self, edit: QLineEdit, callback: object) -> QWidget:
        row = QWidget()
        layout = QHBoxLayout(row)
        layout.setContentsMargins(0, 0, 0, 0)
        button = QPushButton("Examinar…")
        button.clicked.connect(callback)
        layout.addWidget(edit, 1)
        layout.addWidget(button)
        return row

    @Slot()
    def _browse_build(self) -> None:
        selected = QFileDialog.getExistingDirectory(
            self, "Seleccionar build de NativeSharp", self.build_edit.text()
        )
        if selected:
            self.build_edit.setText(selected)

    @Slot()
    def _browse_prefix(self) -> None:
        selected = QFileDialog.getExistingDirectory(
            self, "Seleccionar directorio de instalación", self.prefix_edit.text()
        )
        if selected:
            self.prefix_edit.setText(selected)

    def _paths(self) -> tuple[Path, Path]:
        build_text = self.build_edit.text().strip()
        prefix_text = self.prefix_edit.text().strip()
        if not build_text or not prefix_text:
            raise ValueError("El build y el destino son obligatorios.")
        return resolved(build_text), resolved(prefix_text)

    @Slot()
    def _update_command(self) -> None:
        try:
            build, prefix = self._paths()
            command = ["cmake", "--install", str(build), "--prefix", str(prefix)]
            self.command_label.setText(command_text(command))
        except (OSError, ValueError):
            self.command_label.setText("Seleccione rutas válidas.")

    @Slot()
    def start_deploy(self) -> None:
        if self.process is not None:
            return
        try:
            build, prefix = self._paths()
        except (OSError, ValueError) as exc:
            QMessageBox.warning(self, "NativeSharp Deploy", f"Rutas no válidas:\n{exc}")
            return
        problems = deployment_problems(build, prefix)
        if problems:
            QMessageBox.warning(self, "NativeSharp Deploy", "\n".join(problems))
            return
        answer = QMessageBox.question(
            self,
            "NativeSharp Deploy",
            f"Se instalará el build:\n{build}\n\nen:\n{prefix}\n\n¿Continuar?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
            QMessageBox.StandardButton.No,
        )
        if answer != QMessageBox.StandardButton.Yes:
            return

        self.settings.setValue("build", str(build))
        self.settings.setValue("prefix", str(prefix))
        self.console.clear()
        arguments = ["--install", str(build), "--prefix", str(prefix)]
        self.console.appendPlainText(f"> {command_text(['cmake', *arguments])}\n")
        self.install_button.setEnabled(False)
        self.cancel_button.setEnabled(True)
        self.progress.setRange(0, 0)
        self.status.setText("Instalando…")

        process = QProcess(self)
        process.setProgram(shutil.which("cmake") or "cmake")
        process.setArguments(arguments)
        process.setWorkingDirectory(str(ROOT))
        process.setProcessChannelMode(QProcess.ProcessChannelMode.MergedChannels)
        process.readyReadStandardOutput.connect(self._read_output)
        process.finished.connect(self._finished)
        self.process = process
        process.start()

    @Slot()
    def _read_output(self) -> None:
        if self.process is None:
            return
        data = bytes(self.process.readAllStandardOutput()).decode("utf-8", errors="replace")
        if data:
            self.console.moveCursor(QTextCursor.MoveOperation.End)
            self.console.insertPlainText(data)
            self.console.ensureCursorVisible()

    @Slot(int, QProcess.ExitStatus)
    def _finished(self, exit_code: int, _exit_status: QProcess.ExitStatus) -> None:
        self._read_output()
        self.process = None
        self.install_button.setEnabled(True)
        self.cancel_button.setEnabled(False)
        self.progress.setRange(0, 1)
        self.progress.setValue(1 if exit_code == 0 else 0)
        if exit_code != 0:
            self.status.setText(f"Deploy fallido — código {exit_code}")
            QMessageBox.critical(
                self, "NativeSharp Deploy", f"La instalación terminó con código {exit_code}."
            )
            return

        _, prefix = self._paths()
        compiler = prefix / "bin" / "nsc.exe"
        if not compiler.is_file():
            self.status.setText("Deploy incompleto — falta bin\\nsc.exe")
            QMessageBox.critical(
                self, "NativeSharp Deploy", f"CMake terminó correctamente, pero falta:\n{compiler}"
            )
            return
        try:
            flags = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0
            version = subprocess.run(
                [str(compiler), "--version", "--no-error-log"],
                capture_output=True,
                text=True,
                encoding="utf-8",
                errors="replace",
                creationflags=flags,
                check=False,
            )
            rendered = (version.stdout + version.stderr).strip()
            if rendered:
                self.console.appendPlainText("\n--- nsc instalado ---\n" + rendered + "\n")
            if version.returncode != 0:
                raise RuntimeError(f"nsc --version terminó con código {version.returncode}")
        except (OSError, subprocess.SubprocessError, RuntimeError) as exc:
            self.status.setText("Deploy instalado, verificación fallida")
            QMessageBox.warning(
                self, "NativeSharp Deploy", f"Se instaló, pero no pudo verificarse nsc.exe:\n{exc}"
            )
            return
        self.status.setText(f"Deploy correcto — {prefix}")
        QMessageBox.information(self, "NativeSharp Deploy", f"NativeSharp instalado en:\n{prefix}")

    @Slot()
    def cancel_deploy(self) -> None:
        if self.process is None:
            return
        self.status.setText("Cancelando…")
        self.process.terminate()
        QTimer.singleShot(3000, self._kill_if_running)

    @Slot()
    def _kill_if_running(self) -> None:
        if self.process is not None and self.process.state() != QProcess.ProcessState.NotRunning:
            self.process.kill()

    def closeEvent(self, event: QCloseEvent) -> None:
        if self.process is None:
            event.accept()
            return
        answer = QMessageBox.question(
            self,
            "NativeSharp Deploy",
            "Hay una instalación en curso. ¿Cancelar y cerrar?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
            QMessageBox.StandardButton.No,
        )
        if answer == QMessageBox.StandardButton.Yes:
            self.cancel_deploy()
            event.accept()
        else:
            event.ignore()


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", help="build de CMake")
    parser.add_argument("--prefix", help="directorio de instalación")
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    app = QApplication(sys.argv[:1])
    app.setApplicationName("NativeSharp Deploy")
    app.setStyle("Fusion")
    build = Path(arguments.build) if arguments.build else None
    prefix = Path(arguments.prefix) if arguments.prefix else None
    window = DeployWindow(build, prefix)
    window.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
