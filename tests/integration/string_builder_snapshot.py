"""Verify StringBuilder snapshots and retained process memory."""

import argparse
import ctypes
from ctypes import wintypes
import os
from pathlib import Path
import queue
import shutil
import subprocess
import threading


class ProcessMemoryCounters(ctypes.Structure):
    _fields_ = [("cb", wintypes.DWORD), ("PageFaultCount", wintypes.DWORD)] + [
        (field, ctypes.c_size_t) for field in (
            "PeakWorkingSetSize", "WorkingSetSize", "QuotaPeakPagedPoolUsage",
            "QuotaPagedPoolUsage", "QuotaPeakNonPagedPoolUsage", "QuotaNonPagedPoolUsage",
            "PagefileUsage", "PeakPagefileUsage", "PrivateUsage")
    ]


def check_process(executable, cwd, env):
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    psapi = ctypes.WinDLL("psapi", use_last_error=True)
    kernel.OpenProcess.restype = wintypes.HANDLE
    kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    psapi.GetProcessMemoryInfo.argtypes = [wintypes.HANDLE, ctypes.POINTER(ProcessMemoryCounters), wintypes.DWORD]
    process = subprocess.Popen([str(executable)], cwd=cwd, env=env, stdin=subprocess.PIPE,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    messages = queue.Queue()
    reader = threading.Thread(target=lambda: messages.put(process.stdout.readline()), daemon=True)
    reader.start()
    try:
        try:
            line = messages.get(timeout=30)
        except queue.Empty:
            raise RuntimeError("snapshot fixture did not reach the measurement point")
        if line.strip() != "ready":
            raise RuntimeError(f"snapshot fixture stopped early: {line!r}, exit={process.poll()}")
        handle = kernel.OpenProcess(0x1010, False, process.pid)
        if not handle:
            raise ctypes.WinError(ctypes.get_last_error())
        try:
            counters = ProcessMemoryCounters()
            counters.cb = ctypes.sizeof(counters)
            if not psapi.GetProcessMemoryInfo(handle, ctypes.byref(counters), counters.cb):
                raise ctypes.WinError(ctypes.get_last_error())
            retained = counters.PrivateUsage
        finally:
            kernel.CloseHandle(handle)
        _, error = process.communicate(input="\n", timeout=30)
        if process.returncode:
            raise RuntimeError(f"snapshot fixture exited {process.returncode}: {error}")
        if retained >= 24 * 1024 * 1024:
            raise RuntimeError(f"snapshot fixture retained {retained} private bytes")
        return retained
    finally:
        if process.poll() is None:
            process.kill()
            process.communicate()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--linker", type=Path, required=True)
    parser.add_argument("--runtime", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    options = parser.parse_args()
    root = options.work_dir.resolve()
    root.mkdir(parents=True, exist_ok=False)
    repo = Path(__file__).resolve().parent.parent.parent
    shutil.copytree(repo / "tests" / "cases" / "libraries" / "string_builder_snapshot", root / "probe")
    shutil.copytree(repo / "lib" / "NativeSharp.Core", root / "lib" / "NativeSharp.Core",
                    ignore=shutil.ignore_patterns("obj", "bin"))
    env = os.environ.copy()
    env.update({"TEMP": str(root), "TMP": str(root), "NSC_LINKER": str(options.linker.resolve()),
                "NSC_WINDOWS_RUNTIME_LIBRARY": str(options.runtime.resolve())})
    for optimization in ("-O0", "-O2"):
        executable = root / (optimization[1:] + ".exe")
        result = subprocess.run([str(options.compiler.resolve()), optimization, "--emit-project-oop-exe",
                                 str(root / "probe" / "Probe.nsproj"), "-o", str(executable)],
                                cwd=root, env=env, capture_output=True, text=True)
        if result.returncode:
            raise RuntimeError(f"{optimization}: compilation failed: {result.stderr}")
        retained = check_process(executable, root, env)
        print(f"PASS: snapshots {optimization}, {retained} private bytes", flush=True)


if __name__ == "__main__":
    main()
