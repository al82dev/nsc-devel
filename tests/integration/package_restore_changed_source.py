"""A changed package source must not replace a previously verified store."""

import argparse
import os
from pathlib import Path
import subprocess
import time


def run(command, cwd, environment):
    result = subprocess.run(command, cwd=cwd, env=environment, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(f"{command!r}: {result.returncode}\n{result.stdout}\n{result.stderr}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    options = parser.parse_args()
    compiler = str(options.compiler.resolve())
    root = options.work_dir.resolve()
    root.mkdir(parents=True, exist_ok=False)
    environment = os.environ.copy()
    environment.update({"TEMP": str(root), "TMP": str(root)})
    package = root / "package"
    package.mkdir()
    with (package / "Demo.lib").open("wb") as stream:
        stream.truncate(64 * 1024 * 1024)
    metadata = package / "Demo.nsmeta"
    metadata.write_bytes(b"initial-metadata")
    project_root = root / "project"
    run([compiler, "new", "console", "-n", "Probe", "-o", str(project_root)], root, environment)
    project = project_root / "Probe.nsproj"
    run([compiler, "add", "package", "Demo", "--version", "1.0", "--source", str(package),
         str(project)], root, environment)
    run([compiler, "restore", str(project)], root, environment)
    installed = project_root / "obj" / "nativesharp" / "packages" / "Demo" / "1.0" / "Demo.nsmeta"
    process = subprocess.Popen([compiler, "restore", str(project), "--force"], cwd=root, env=environment,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    staging = project_root / "obj" / "nativesharp" / ("packages.stage." + str(process.pid))
    changed = False
    while process.poll() is None:
        if not changed and staging.is_dir():
            metadata.write_bytes(b"changed-metadata")
            changed = True
        time.sleep(0.001)
    output, error = process.communicate()
    (root / "restore.stdout").write_bytes(output)
    (root / "restore.stderr").write_bytes(error)
    if not changed or process.returncode == 0 or b"restore verification found stale artifact" not in error:
        raise RuntimeError(f"source change was not caught: changed={changed}, exit={process.returncode}, {error!r}")
    if installed.read_bytes() != b"initial-metadata":
        raise RuntimeError("failed restore replaced the previously published metadata")
    print("PASS: changed source rejected before publication", flush=True)


if __name__ == "__main__":
    main()
