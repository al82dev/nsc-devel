"""Comprueba rechazos de records C++ fuera de la ruta variadica SysV probada."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
args = parser.parse_args()
source = Path(__file__).resolve().parent
root = Path(tempfile.mkdtemp(prefix='nsc-t09-cpp-var-record-indirecta-negativos-', dir=r'A:\temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
for name, transform in (
    ('cdecl', lambda s: s.replace('int (__attribute__((sysv_abi)) *)(int,...)', 'int (*)(int,...)')),
    ('no-trivial', lambda s: s.replace('double x; int y;', 'double x; int y; ~T09VarValue();')),
):
    case = root / name
    case.mkdir()
    for filename in ('api.h', 'api.cpp', 'Main.ns', 'VectorCall.nsproj'):
        shutil.copyfile(source / filename, case / filename)
    for label, command in (
        ('clang', ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-c', 'api.cpp', '-o', 'api.obj']),
        ('lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj']),
    ):
        result = subprocess.run(command, cwd=case, env=env, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, text=True, errors='replace')
        (root / (name + '-' + label + '.log')).write_text(repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
        if result.returncode:
            raise SystemExit(name + ': fallo al preparar ' + label + '; evidencia: ' + str(root))
    header = case / 'api.h'
    original = header.read_text(encoding='utf-8')
    changed = transform(original)
    if changed == original:
        raise SystemExit(name + ': no se altero el fixture')
    header.write_text(changed, encoding='utf-8')
    command = [args.nsc, '--dump-project-oop-llvm', str(case / 'VectorCall.nsproj'),
               '--library-reference-root', str(case), '--cpp-clang-root=' + args.clang_root]
    result = subprocess.run(command, cwd=case, env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    (root / (name + '.log')).write_text(repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
    if result.returncode == 0 or 'variadic function-pointer arguments must be' not in result.stdout:
        raise SystemExit(name + ': debio rechazarse; evidencia: ' + str(root))
    print('OK: rechazo', name, flush=True)
print('Evidencia:', root)