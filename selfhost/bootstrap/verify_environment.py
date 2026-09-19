#!/usr/bin/env python3
"""Verifica que el entorno físico coincide con bootstrap.lock, sin modificarlo."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

PLACEHOLDER = re.compile(r"\$\{([A-Za-z_][A-Za-z0-9_]*)\}")


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def parse_assignments(values: list[str]) -> dict[str, str]:
    result: dict[str, str] = {}
    for value in values:
        name, separator, content = value.partition("=")
        if not separator or not name or not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", name):
            raise ValueError(f"asignación inválida: {value!r}; se esperaba NOMBRE=valor")
        result[name] = content
    return result


def expand_path(template: str, variables: dict[str, str]) -> Path:
    missing: list[str] = []

    def replace(match: re.Match[str]) -> str:
        name = match.group(1)
        value = variables.get(name, "")
        if not value:
            missing.append(name)
            return match.group(0)
        return value

    expanded = PLACEHOLDER.sub(replace, template)
    if missing:
        raise ValueError("variables sin definir: " + ", ".join(sorted(set(missing))))
    return Path(expanded).resolve()


def load_lock(path: Path) -> dict:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ValueError("bootstrap.lock debe contener un objeto JSON")
    if value.get("schema") != "nativesharp-bootstrap-lock" or value.get("version") != 1:
        raise ValueError("bootstrap.lock usa un schema o versión desconocidos")
    artifacts = value.get("artifacts")
    if not isinstance(artifacts, list) or not artifacts:
        raise ValueError("bootstrap.lock no contiene artefactos")
    identifiers = [item.get("id") for item in artifacts if isinstance(item, dict)]
    if len(identifiers) != len(artifacts) or len(set(identifiers)) != len(identifiers):
        raise ValueError("cada artefacto debe tener un id único")
    return value


def capture_version(path: Path, arguments: list[str]) -> tuple[int, str]:
    flags = getattr(subprocess, "CREATE_NO_WINDOW", 0) if os.name == "nt" else 0
    completed = subprocess.run(
        [str(path), *arguments],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
        creationflags=flags,
        timeout=30,
        check=False,
    )
    return completed.returncode, completed.stdout.strip()


def verify_artifact(item: dict, variables: dict[str, str]) -> dict:
    identifier = str(item.get("id", ""))
    row: dict = {"id": identifier, "status": "failed", "problems": []}
    if item.get("state") != "locked":
        row["problems"].append(f"estado no bloqueado: {item.get('state')!r}")
    expected_hash = item.get("sha256")
    if not isinstance(expected_hash, str) or re.fullmatch(r"[0-9a-f]{64}", expected_hash) is None:
        row["problems"].append("SHA-256 pendiente o inválido")
    try:
        path = expand_path(str(item.get("path", "")), variables)
    except ValueError as exc:
        row["problems"].append(str(exc))
        return row
    row["path"] = str(path)
    if not path.is_file():
        row["problems"].append("el archivo no existe")
        return row
    row["size"] = path.stat().st_size
    actual_hash = sha256_file(path)
    row["sha256"] = actual_hash
    if isinstance(expected_hash, str) and actual_hash != expected_hash:
        row["problems"].append(f"SHA-256 distinto: {actual_hash}")
    version_arguments = item.get("version_args", [])
    expected_version = item.get("version_contains")
    if version_arguments or expected_version:
        if not isinstance(version_arguments, list) or not all(
            isinstance(argument, str) for argument in version_arguments
        ):
            row["problems"].append("version_args inválido")
        else:
            try:
                exit_code, output = capture_version(path, version_arguments)
                row["version_exit_code"] = exit_code
                row["version_output"] = output
                if exit_code != 0:
                    row["problems"].append(f"consulta de versión terminó con {exit_code}")
                if isinstance(expected_version, str) and expected_version not in output:
                    row["problems"].append(
                        f"la versión no contiene el texto esperado: {expected_version!r}"
                    )
            except (OSError, subprocess.SubprocessError) as exc:
                row["problems"].append(f"no se pudo consultar la versión: {exc}")
    row["status"] = "passed" if not row["problems"] else "failed"
    return row


def verify(lock: dict, variables: dict[str, str]) -> dict:
    artifacts = [verify_artifact(item, variables) for item in lock["artifacts"]]
    problems = [
        f"{item['id']}: {problem}"
        for item in artifacts
        for problem in item.get("problems", [])
    ]
    return {
        "schema": "nativesharp-bootstrap-environment-report",
        "version": 1,
        "verified_at": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        "status": "passed" if not problems else "failed",
        "target": lock.get("target", {}),
        "artifacts": artifacts,
        "problems": problems,
    }


def write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    temporary.replace(path)


def main() -> int:
    root = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lock", type=Path, default=root / "bootstrap.lock")
    parser.add_argument("--set", action="append", default=[], metavar="NAME=VALUE")
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    try:
        variables = dict(os.environ)
        variables.update(parse_assignments(args.set))
        lock = load_lock(args.lock)
        report = verify(lock, variables)
        if args.report is not None:
            write_json(args.report, report)
    except (OSError, json.JSONDecodeError, ValueError) as exc:
        print(f"bootstrap environment: error: {exc}", file=sys.stderr)
        return 2
    for item in report["artifacts"]:
        print(f"{item['status']:6} {item['id']}: {item.get('path', '<sin ruta>')}")
        for problem in item["problems"]:
            print(f"       {problem}")
    print(f"bootstrap environment: {report['status']}")
    return 0 if report["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
