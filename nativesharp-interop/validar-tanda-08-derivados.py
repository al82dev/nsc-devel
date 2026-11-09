"""Runs of three tanda 08 validators from a CRLF copy of the test trees.

Until T08-D20 these validators could not pass unchanged and this script ran them from a copy with the documented
change applied: CRLF line endings (T08-D08) and, for
pruebas-tanda-08-record-c\\precompiled-bool\\validar-cache.cmd and pruebas-tanda-08-tls-c\\atomic\\validar.cmd, the
current artifact-cache contract instead of the diagnostic retired by 03bf12e42. T08-D20 applied those changes to
the versioned validators, so the copy no longer changes anything: the run checks that the validators pass from any
CRLF checkout. / Desde T08-D20 los validadores versionados ya llevan esos cambios y la copia no cambia nada.
"""
from pathlib import Path
import shutil
import subprocess
import sys

source_root = Path(__file__).resolve().parent
target_root = Path(r"A:\nsc-interop-build\t08-derivados\nativesharp-interop")
# Whole test trees: the validators reach sibling fixtures through `%~dp0..\`.
TREES = sorted(p.name for p in source_root.iterdir() if p.is_dir() and p.name.startswith("pruebas-"))
CHANGES = {
    r"pruebas-tanda-08-identidad-c\validar.cmd": [],
    r"pruebas-tanda-08-record-c\precompiled-bool\validar-cache.cmd": [],
    r"pruebas-tanda-08-tls-c\atomic\validar.cmd": [],
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
