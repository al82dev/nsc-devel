"""Contrasta una llamada C++ SysV variádica con parámetro puntero noescape."""
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
args = parser.parse_args()
source = Path(__file__).resolve().parent
root = Path(tempfile.mkdtemp(prefix='nsc-t09-variadic-noescape-', dir='A:/temp'))
fixture = root / 'fixture'
shutil.copytree(source, fixture)
def replace_once(name, before, after):
    path = fixture / name
    text = path.read_text(encoding='utf-8')
    if text.count(before) != 1:
        raise SystemExit('no se encontro el control esperado en ' + name)
    path.write_text(text.replace(before, after), encoding='utf-8')
replace_once('api.h', 'int __attribute__((sysv_abi)) sum(int count,...)',
             'int __attribute__((sysv_abi)) sum(int* __attribute__((noescape)) count,...)')
replace_once('api.cpp', 'int sum(int count,...)', 'int sum(int* __attribute__((noescape)) count,...)')
replace_once('api.cpp', 'cmpl $3, %edi', 'cmpl $3, (%rdi)')
for name in ('Main.ns', 'Producer.ns'):
    replace_once(name, 'return T09Cpp.sum(3,', 'int count=3; return T09Cpp.sum(&count,')
replace_once('Control.cpp', 'return T09Cpp::sum(3,', 'int count=3; return T09Cpp::sum(&count,')
for name in ('validar.py', 'validar-precompilado.py'):
    path = fixture / name
    text = path.read_text(encoding='utf-8')
    if '?sum@T09Cpp@@YAHHZZ' not in text:
        raise SystemExit('no se encontro el simbolo de control: ' + name)
    text = text.replace('?sum@T09Cpp@@YAHHZZ', '?sum@T09Cpp@@YAHPEAHZZ')
    text = text.replace(r'\(i32,\s*\.\.\.\)', r'\(ptr,\s*\.\.\.\)')
    path.write_text(text, encoding='utf-8')
command = ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-Xclang', '-ast-dump',
           '-fsyntax-only', str(fixture / 'api.cpp')]
result = subprocess.run(command, cwd=fixture, env=os.environ.copy(), stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT, text=True, errors='replace')
(root / 'oracle-ast.log').write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
if result.returncode or 'NoEscapeAttr' not in result.stdout:
    raise SystemExit('Clang no observo el atributo: ' + str(root))
for name in ('validar.py', 'validar-precompilado.py'):
    command = [sys.executable, str(fixture / name), '--nsc', args.nsc, '--clang-root', args.clang_root]
    result = subprocess.run(command, cwd=fixture, env=os.environ.copy(), stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    (root / (name + '.log')).write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
    print(result.stdout, flush=True)
    if result.returncode:
        raise SystemExit(name + ' fallo; evidencia: ' + str(root))
print('OK: llamada variadica SysV con declaracion noescape; evidencia:', root)
