"""Contrasta reservas de pila directas para funciones C con Clang."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
parser.add_argument('--sysv', action='store_true')
args = parser.parse_args()
source = Path(__file__).resolve().parent
root = Path(tempfile.mkdtemp(prefix='nsc-t09-c-size-stackalloc-', dir='A:/temp'))
fixture = root / 'fixture'
shutil.copytree(source, fixture)

def replace(name, old, new):
    path = fixture / name
    text = path.read_text(encoding='utf-8')
    if text.count(old) != 1:
        raise SystemExit('control no encontrado: ' + name)
    path.write_text(text.replace(old, new), encoding='utf-8')

path = fixture / 'api.h'
path.write_text(path.read_text(encoding='utf-8') + '\nT09_STORAGE int t09_index(void);\n', encoding='utf-8')
path = fixture / 'api.c'
path.write_text(path.read_text(encoding='utf-8') + '\nint t09_index(void) { return 1; }\n', encoding='utf-8')
ns_checks = ['    int count = t09_index()+2;']
c_checks = ['    int count = t09_index()+2;']
for name, variable, constant in (('t09_s0', '-1', '12'), ('t09_s1', '-1', '12'),
        ('t09_s2', '0', '12'), ('t09_s3', '0', '0'), ('t09_d0', '12', '12'),
        ('t09_d1', '12', '12'), ('t09_d2', '12', '12'), ('t09_d3', '0', '0')):
    ns_checks.append(f'    if ({name}(stackalloc int[count]) != {variable} || '
                     f'{name}(stackalloc int[3]) != {constant}) return 40;')
    c_checks.append(f'    if ({name}((int*)__builtin_alloca(count*sizeof(int))) != {variable} || '
                    f'{name}((int*)__builtin_alloca(3*sizeof(int))) != {constant}) return 40;')
for name in ('Main.ns', 'Producer.ns'):
    replace(name, '    return 0;', '\n'.join(ns_checks) + '\n    return 0;')
replace('Control.c', '    return 0;', '\n'.join(c_checks) + '\n    return 0;')
replace('validar.py', "flags = ['-target', 'x86_64-pc-windows-msvc', '-std=c11', '-O' + str(opt)]",
        "flags = ['-target', 'x86_64-pc-windows-msvc', '-std=c11', '-mno-stack-arg-probe', '-O' + str(opt)]")
for script in ('validar.py', 'validar-precompilado.py'):
    command = [sys.executable, str(fixture / script), '--nsc', args.nsc, '--clang-root', args.clang_root]
    if args.sysv:
        command.append('--sysv')
    result = subprocess.run(command, cwd=fixture, env=os.environ.copy(), stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    (root / (script + '.log')).write_text('COMMAND: ' + repr(command) + '\nEXIT: '
        + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
    print(script, result.stdout, flush=True)
    if result.returncode:
        raise SystemExit('fallo; evidencia: ' + str(root))
print('OK: reservas de pila directas para C; evidencia:', root)
