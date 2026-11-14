"""Contrasta reservas de pila directas con Clang y plantillas precompiladas."""
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
root = Path(tempfile.mkdtemp(prefix='nsc-t09-size-stackalloc-', dir='A:/temp'))
fixture = root / 'fixture'
shutil.copytree(source, fixture)

def replace(name, old, new):
    path = fixture / name
    text = path.read_text(encoding='utf-8')
    if text.count(old) != 1:
        raise SystemExit('control no encontrado: ' + name)
    path.write_text(text.replace(old, new), encoding='utf-8')

ns_checks = ['    int count = T09ObjectSize.index()+2;']
cpp_checks = ['    int count = T09ObjectSize::index()+2;']
for name, variable, constant in (('size0', '-1', '12'), ('size1', '-1', '12'),
        ('size2', '0', '12'), ('size3', '0', '0'), ('dynamic0', '12', '12'),
        ('dynamic1', '12', '12'), ('dynamic2', '12', '12'), ('dynamic3', '0', '0')):
    ns_checks.append(f'    if (T09ObjectSize.{name}(stackalloc int[count]) != {variable} || '
                     f'T09ObjectSize.{name}(stackalloc int[3]) != {constant}) return 40;')
    cpp_checks.append(f'    if (T09ObjectSize::{name}((int*)__builtin_alloca(count*sizeof(int))) != {variable} || '
                      f'T09ObjectSize::{name}((int*)__builtin_alloca(3*sizeof(int))) != {constant}) return 40;')
for name in ('Main.ns', 'Producer.ns'):
    replace(name, '    return 0;', '\n'.join(ns_checks) + '\n    return 0;')
replace('Control.cpp', '    return T09ObjectSize::size0', '\n'.join(cpp_checks) + '\n    return T09ObjectSize::size0')
replace('validar.py', "flags = ['-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-O' + str(opt)]",
        "flags = ['-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-mno-stack-arg-probe', '-O' + str(opt)]")
path = fixture / 'validar.py'
text = path.read_text(encoding='utf-8')
start = text.index('    if not args.solo_extremos:\n        for name in ')
end = text.index('    if opt == 0:', start)
path.write_text(text[:start] + text[end:], encoding='utf-8')

for script in ('validar.py', 'validar-precompilado.py'):
    for dll in (False, True):
        command = [sys.executable, str(fixture / script), '--nsc', args.nsc,
                   '--clang-root', args.clang_root, '--const-parametros']
        if dll:
            command.append('--dll')
        if args.sysv:
            command.append('--sysv')
        result = subprocess.run(command, cwd=fixture, env=os.environ.copy(), stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, text=True, errors='replace')
        label = script.replace('.py', '') + ('-dll' if dll else '-estatica')
        (root / (label + '.log')).write_text('COMMAND: ' + repr(command) + '\nEXIT: '
            + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
        print(label, result.stdout, flush=True)
        if result.returncode:
            raise SystemExit('fallo; evidencia: ' + str(root))
print('OK: reservas de pila directas; evidencia:', root)
