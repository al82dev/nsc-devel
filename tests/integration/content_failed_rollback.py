"""Exercise Content publication rollback when the destination becomes occupied."""

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
    parser.add_argument("--linker", type=Path, required=True)
    parser.add_argument("--runtime", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    options = parser.parse_args()
    root = options.work_dir.resolve()
    root.mkdir(parents=True, exist_ok=False)
    compiler = str(options.compiler.resolve())
    env = os.environ.copy()
    env.update({"TEMP": str(root), "TMP": str(root), "NSC_LINKER": str(options.linker.resolve()),
                "NSC_WINDOWS_RUNTIME_LIBRARY": str(options.runtime.resolve())})
    project_root = root / "project"
    run([compiler, "new", "console", "-n", "Probe", "-o", str(project_root)], root, env)
    content = project_root / "Content"
    content.mkdir()
    source = content / "example.txt"
    source.write_bytes(b"previous content")
    project = project_root / "Probe.nsproj"
    run([compiler, "build", str(project), "--no-restore"], root, env)
    source.write_bytes(b"updated content")
    target = project_root / "bin" / "Debug" / "Content"
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
        process = subprocess.run([compiler, "build", str(project), "--no-restore"], cwd=root,
                                 env=env, capture_output=True, text=True)
    finally:
        stop.set()
        for thread in threads:
            thread.join()
    (root / "build.stdout").write_text(process.stdout)
    (root / "build.stderr").write_text(process.stderr)
    backups = list(target.parent.glob(".nspm.Content.backup.*"))
    if not created.is_set() or process.returncode == 0 or "cannot publish Content" not in process.stderr:
        raise RuntimeError(f"did not reach publication race: created={created.is_set()}, "
                           f"exit={process.returncode}, {process.stderr}")
    if len(backups) != 1 or (backups[0] / "example.txt").read_bytes() != b"previous content":
        raise RuntimeError(f"previous content was not preserved: {backups}")
    if "rollback" not in process.stderr or str(backups[0]) not in process.stderr:
        raise RuntimeError(f"rollback failure and backup path not reported: {process.stderr}")
    print("PASS: failed Content rollback reports preserved backup", flush=True)


if __name__ == "__main__":
    main()
