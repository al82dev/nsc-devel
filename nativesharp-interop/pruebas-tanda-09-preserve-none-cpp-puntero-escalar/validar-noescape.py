"""Prueba una llamada C++ preserve_none con un parámetro noescape."""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
args = parser.parse_args()
source = Path(__file__).resolve().parent
root = Path(tempfile.mkdtemp(prefix='nsc-t09-preserve-noescape-', dir='A:/temp'))
fixture = root / 'fixture'
shutil.copytree(source, fixture)
for name in ('api.h', 'api.cpp'):
    path = fixture / name
    data = path.read_text(encoding='utf-8')
    old = 'scalar(float x,int* p)'
    new = 'scalar(float x,int* __attribute__((noescape)) p)'
    if data.count(old) != 1:
        raise SystemExit('no se encontro la firma de control: ' + name)
    path.write_text(data.replace(old, new), encoding='utf-8')
command = ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-O0', '-S', '-emit-llvm',
           str(fixture / 'api.cpp'), '-o', str(root / 'oracle.ll')]
result = subprocess.run(command, cwd=fixture, env=os.environ.copy(), stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT, text=True, errors='replace')
(root / 'oracle.log').write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
if result.returncode:
    raise SystemExit('fallo el control Clang: ' + str(root))
ir = (root / 'oracle.ll').read_text(encoding='utf-8')
if not re.search(r'define[^\n]*preserve_nonecc[^\n]*scalar@T09Cpp[^\n]*captures\(', ir):
    raise SystemExit('Clang no emitio el atributo de captura esperado: ' + str(root))
for name in ('validar.py', 'validar-precompilado.py'):
    command = [sys.executable, str(fixture / name), '--nsc', args.nsc, '--clang-root', args.clang_root]
    result = subprocess.run(command, cwd=fixture, env=os.environ.copy(), stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    (root / (name + '.log')).write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
    print(result.stdout, flush=True)
    if result.returncode:
        raise SystemExit(name + ' fallo; evidencia: ' + str(root))
    match = re.search(r'^Evidencia:\s*(.+)$', result.stdout, re.MULTILINE)
    if not match:
        raise SystemExit('falta directorio de evidencia: ' + str(root))
    output = Path(match.group(1).strip())
    if name == 'validar.py':
        ir = (output / 'estatica-O0-ns-ir.log').read_text(encoding='utf-8')
    else:
        consumer = output / 'estatica-O0' / 'sin-encabezado'
        ir_command = [args.nsc, '--dump-project-oop-llvm', str(consumer / 'Consumer.nsproj'),
                      '--cpp-clang-root=' + args.clang_root]
        result = subprocess.run(ir_command, cwd=consumer, env=os.environ.copy(), stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, text=True, errors='replace')
        (root / 'precompilado-ir.log').write_text('EXIT: ' + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
        if result.returncode:
            raise SystemExit('fallo el IR precompilado: ' + str(root))
        ir = result.stdout
    scalar_lines = [line for line in ir.splitlines() if 'scalar@T09Cpp' in line and
                    ('declare ' in line or 'call ' in line)]
    if len(scalar_lines) < 2 or any(re.search(r'\bcaptures\(', line) for line in scalar_lines):
        raise SystemExit('NSC emitio una promesa de captura inesperada o falta la llamada: ' + str(root))
print('OK: llamada C++ con noescape; evidencia:', root)
