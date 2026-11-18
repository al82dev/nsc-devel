"""Report a published package store whose backup cannot be removed (Windows)."""

import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time


def run(command, cwd, environment):
    result = subprocess.run(command, cwd=cwd, env=environment, capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(f"{command!r}: {result.returncode}\n{result.stdout}\n{result.stderr}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    options = parser.parse_args()
    root = options.work_dir.resolve()
    root.mkdir(parents=True, exist_ok=False)
    compiler = str(options.compiler.resolve())
    env = os.environ.copy()
    env.update({"TEMP": str(root), "TMP": str(root)})
    package = root / "package"
    package.mkdir()
    (package / "Demo.lib").write_bytes(b"library")
    (package / "Demo.nsmeta").write_bytes(b"metadata")
    project_root = root / "project"
    run([compiler, "new", "console", "-n", "Probe", "-o", str(project_root)], root, env)
    project = project_root / "Probe.nsproj"
    run([compiler, "add", "package", "Demo", "--version", "1.0", "--source", str(package),
         str(project)], root, env)
    run([compiler, "restore", str(project)], root, env)
    (package / "Demo.nsmeta").write_bytes(b"new-metadata")

    parent = project_root / "obj" / "nativesharp"
    extra = parent / "packages" / "extra"
    extra.mkdir()
    for index in range(4000):
        (extra / f"f{index:05d}").touch()
    (extra / "zzzz-sentinel").touch()
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.CreateFileW.restype = ctypes.c_void_p
    kernel.CreateFileW.argtypes = [ctypes.c_wchar_p, ctypes.c_uint32, ctypes.c_uint32,
                                   ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p]
    kernel.CloseHandle.argtypes = [ctypes.c_void_p]
    process = subprocess.Popen([compiler, "restore", str(project), "--force"], cwd=root, env=env,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    backup = parent / f"packages.backup.{process.pid}"
    sentinel = backup / "extra" / "zzzz-sentinel"
    handle = None
    while process.poll() is None:
        if sentinel.exists():
            candidate = kernel.CreateFileW(str(sentinel), 0x80000000, 3, None, 3, 0, None)
            if candidate != ctypes.c_void_p(-1).value:
                handle = candidate
                break
        time.sleep(0.0005)
    output, error = process.communicate()
    if handle is not None:
        kernel.CloseHandle(handle)
    (root / "restore.stdout").write_bytes(output)
    (root / "restore.stderr").write_bytes(error)
    if handle is None:
        raise RuntimeError("failed to hold a file in the backup before cleanup")
    if process.returncode == 0 or b"published" not in error or b"backup" not in error:
        raise RuntimeError(f"cleanup failure not reported: exit={process.returncode}, {error!r}")
    if not backup.exists() or not (parent / "packages" / "Demo" / "1.0" / "Demo.lib").exists():
        raise RuntimeError("expected published package and surviving backup")
    installed = parent / "packages" / "Demo" / "1.0" / "Demo.nsmeta"
    data = installed.read_bytes()
    lock = json.loads((project_root / "NativeSharp.lock.json").read_text())
    artifact = next(item for item in lock["packages"][0]["artifacts"] if item["path"].endswith("Demo.nsmeta"))
    if data != b"new-metadata" or artifact["size"] != len(data) or artifact["sha256"] != hashlib.sha256(data).hexdigest():
        raise RuntimeError("published store and lock do not match")
    print("PASS: backup cleanup failure is reported with matching published store and lock", flush=True)


if __name__ == "__main__":
    main()
