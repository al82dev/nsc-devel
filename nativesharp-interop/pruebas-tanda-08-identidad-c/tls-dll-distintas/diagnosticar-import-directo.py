from pathlib import Path
import os
import shutil
import subprocess
import sys

root = Path(r"A:\nsc-interop-build\t08-tls-dos-dll-repro")
test = root / "control-import-directo"
test.mkdir(exist_ok=True)
source = Path(__file__).resolve().parent / "ControlImportDirecto.c"
original = test / "api.dll"
alias = test / "alias.dll"
alias.unlink(missing_ok=True)
shutil.copyfile(root / "uno" / "api.dll", original)
commands = [
    ["clang.exe", "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", str(source), "-o", "control.obj"],
    ["lld-link.exe", "/nologo", "/entry:mainCRTStartup", "/subsystem:console", "/nodefaultlib",
     "/out:control.exe", "control.obj", str(root / "uno" / "api.lib"),
     r"A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib"],
]
for command in commands:
    result = subprocess.run(command, cwd=test, capture_output=True, text=True, errors="replace")
    if result.returncode:
        print(result.stdout + result.stderr)
        sys.exit(result.returncode)
for mode in ("copy", "hardlink"):
    alias.unlink(missing_ok=True)
    if mode == "copy":
        shutil.copyfile(original, alias)
    else:
        os.link(original, alias)
    process = subprocess.run([str(test / "control.exe")], cwd=Path("A:/"))
    if process.returncode not in (0, 7):
        print(mode, "error", process.returncode)
        sys.exit(1)
    print(mode, "same-module" if process.returncode == 0 else "different-modules")
    expected = 7 if mode == "copy" else 0
    if process.returncode != expected:
        print("ERROR: la identidad del modulo no coincide con el control esperado")
        sys.exit(1)
