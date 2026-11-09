"""Derived runs of three historical tanda 08 validators that cannot pass unchanged.

The historical scripts stay untouched. Each one is run from a scratch copy of the test trees whose .cmd files are
converted to CRLF (cmd's label search needs it), with only the documented change applied to the copy:

1. pruebas-tanda-08-identidad-c\\validar.cmd: no change; the versioned file has LF endings and cmd cannot find its
   `setup_atomic` label.
2. pruebas-tanda-08-record-c\\precompiled-bool\\validar-cache.cmd and 3. pruebas-tanda-08-tls-c\\atomic\\validar.cmd:
   they expect the retired diagnostic "C/C++ project references require --no-artifact-cache until transitive headers
   are tracked" (removed by 03bf12e42). The copy requires the current contract instead: the cached build succeeds,
   and for the atomic suite the cached executable runs and a second build reports a cache hit.
Run the historical validar-tanda-08.cmd as well; this file documents and checks the difference.
"""
from pathlib import Path
import shutil
import subprocess
import sys

source_root = Path(__file__).resolve().parent
target_root = Path(r"A:\nsc-interop-build\t08-derivados\nativesharp-interop")
# Whole test trees: the validators reach sibling fixtures through `%~dp0..\`.
TREES = sorted(p.name for p in source_root.iterdir() if p.is_dir() and p.name.startswith("pruebas-"))
RETIRED = "C/C++ project references require --no-artifact-cache until transitive headers are tracked"
CHANGES = {
    r"pruebas-tanda-08-identidad-c\validar.cmd": [],
    r"pruebas-tanda-08-record-c\precompiled-bool\validar-cache.cmd": [
        ('if not errorlevel 1 exit /b 1\r\nfindstr /l /c:"' + RETIRED +
         '" "%DEST%\\producer-first-O%~1.log" >nul\r\nif errorlevel 1 (type "%DEST%\\producer-first-O%~1.log" & '
         'exit /b 1)',
         'if errorlevel 1 (type "%DEST%\\producer-first-O%~1.log" & exit /b 1)'),
    ],
    r"pruebas-tanda-08-tls-c\atomic\validar.cmd": [
        ('if not errorlevel 1 goto :failure\r\nfindstr /L /C:"' + RETIRED +
         '" "%DEST%\\cache.log" >nul\r\nif errorlevel 1 goto :failure',
         'if errorlevel 1 goto :failure\r\n"%DEST%\\cached.exe"\r\nif errorlevel 1 goto :failure\r\n'
         'nsc.exe --cpp-clang-root=A:\\al82dev-clang --emit-project-oop-exe "%DEST%\\Project.nsproj" '
         '--artifact-cache="%DEST%\\cache" -O0 --time -o "%DEST%\\cached.exe" > "%DEST%\\cache-hit.log" 2>&1\r\n'
         'if errorlevel 1 goto :failure\r\n'
         'findstr /L /C:"compilation-bundle-cache: hit" "%DEST%\\cache-hit.log" >nul\r\n'
         'if errorlevel 1 goto :failure'),
    ],
}


def copy_trees():
    for name in TREES:
        target = target_root / name
        if target.exists():
            shutil.rmtree(target)
        shutil.copytree(source_root / name, target)
        for script in target.rglob("*.cmd"):
            data = script.read_bytes().replace(b"\r\n", b"\n").replace(b"\n", b"\r\n")
            script.write_bytes(data)


def apply_changes(relative, changes):
    script = target_root / relative
    text = script.read_bytes().decode("utf-8")
    for old, new in changes:
        if text.count(old) != 1:
            raise AssertionError(f"{relative}: el fragmento a sustituir no aparece una sola vez:\n{old}")
        text = text.replace(old, new)
    script.write_bytes(text.encode("utf-8"))


def main():
    copy_trees()
    failed = []
    for relative, changes in CHANGES.items():
        apply_changes(relative, changes)
        result = subprocess.run(["cmd.exe", "/d", "/c", str(target_root / relative)], cwd=r"A:\nsc-devel",
                                text=True, encoding="utf-8", errors="replace", stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT)
        status = "correcto" if result.returncode == 0 else f"FALLO ({result.returncode})"
        print(f"derivado {relative}: {status}", flush=True)
        if result.returncode:
            failed.append(relative)
            print(result.stdout[-4000:], flush=True)
    if failed:
        sys.exit(1)
    print("Validadores derivados de la tanda 08 correctos.")


main()
