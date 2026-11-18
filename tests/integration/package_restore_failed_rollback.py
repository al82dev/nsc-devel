"""Exercise a failed restore rollback after a competing destination appears."""

import argparse
import ctypes
import os
from pathlib import Path
import subprocess
import threading


def run(command, cwd, env):
    result = subprocess.run(command, cwd=cwd, env=env, capture_output=True, text=True)
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
    (package / "Demo.lib").write_bytes(b"original-library")
    (package / "Demo.nsmeta").write_bytes(b"original-metadata")
    project_root = root / "project"
    run([compiler, "new", "console", "-n", "Probe", "-o", str(project_root)], root, env)
    project = project_root / "Probe.nsproj"
    run([compiler, "add", "package", "Demo", "--version", "1.0", "--source", str(package),
         str(project)], root, env)
    run([compiler, "restore", str(project)], root, env)
    lock_before = (project_root / "NativeSharp.lock.json").read_bytes()
    parent = project_root / "obj" / "nativesharp"
    target = parent / "packages"
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.CreateDirectoryW.argtypes = [ctypes.c_wchar_p, ctypes.c_void_p]
    kernel.CreateDirectoryW.restype = ctypes.c_int
    created = threading.Event()
    stop = threading.Event()

    def compete():
        while not stop.is_set():
            if kernel.CreateDirectoryW(str(target), None):
                created.set()
                break

    threads = [threading.Thread(target=compete) for _ in range(8)]
    for thread in threads:
        thread.start()
    try:
        process = subprocess.run([compiler, "restore", str(project), "--force"], cwd=root,
                                 env=env, capture_output=True, text=True)
    finally:
        stop.set()
        for thread in threads:
            thread.join()
    (root / "restore.stdout").write_text(process.stdout)
    (root / "restore.stderr").write_text(process.stderr)
    backups = list(parent.glob("packages.backup.*"))
    if not created.is_set() or process.returncode == 0 or "cannot publish" not in process.stderr:
        raise RuntimeError(f"failed to reach publication race: created={created.is_set()}, "
                           f"exit={process.returncode}, {process.stderr}")
    if len(backups) != 1:
        raise RuntimeError(f"expected one backup: {backups}")
    previous = backups[0] / "Demo" / "1.0" / "Demo.lib"
    if previous.read_bytes() != b"original-library" or (project_root / "NativeSharp.lock.json").read_bytes() != lock_before:
        raise RuntimeError("previous backup or lock was not preserved")
    if "rollback" not in process.stderr or str(backups[0]) not in process.stderr:
        raise RuntimeError(f"rollback failure and backup path not reported: {process.stderr}")
    print("PASS: failed rollback reports preserved backup", flush=True)


if __name__ == "__main__":
    main()
