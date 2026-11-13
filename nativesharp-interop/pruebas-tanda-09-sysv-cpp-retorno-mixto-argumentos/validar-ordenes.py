"""Comprueba distintos ordenes de escalares y record en llamadas SysV C++."""
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
root = Path(tempfile.mkdtemp(prefix='nsc-t09-retorno-mixto-ordenes-', dir=r'A:\temp'))
variants = {
    'entero-primero': ('int, Small, float', 'int bias, Small p, float extra', '5,s,2.0f', 'i32'),
    'flotante-primero': ('float, int, Small', 'float extra, int bias, Small p', '2.0f,5,s', 'float'),
}
old_most = '?most@T09Cpp@@YA?AUSmall@1@U21@HM@Z'
old_fetch = '?fetchCallback@T09Cpp@@YAP6A?AUSmall@1@U21@HM@ZXZ'
print('Evidencia:', root, flush=True)
for label, (declaration, definition, arguments, first) in variants.items():
    dest = root / label
    shutil.copytree(source, dest)
    header = dest / 'api.h'
    header.write_text(header.read_text().replace('Small, int, float', declaration))
    implementation = dest / 'api.cpp'
    implementation.write_text(implementation.read_text().replace('Small p, int bias, float extra', definition))
    for name in ('Main.ns', 'Producer.ns', 'Control.cpp'):
        path = dest / name
        path.write_text(path.read_text().replace('(s,5,2.0f)', '(' + arguments + ')'))
    obj = dest / 'probe.obj'
    commands = [
        ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-c', str(implementation), '-o', str(obj)],
        ['llvm-nm.exe', '--defined-only', str(obj)],
    ]
    for command in commands:
        output = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
        if output.returncode:
            raise SystemExit(label + ': fallo de sonda: ' + output.stdout)
    symbols = [line.split()[-1] for line in output.stdout.splitlines() if line.split()]
    most = [value for value in symbols if value.startswith('?most@T09Cpp@@')]
    fetch = [value for value in symbols if value.startswith('?fetchCallback@T09Cpp@@')]
    if len(most) != 1 or len(fetch) != 1:
        raise SystemExit(label + ': simbolos inesperados: ' + repr(symbols))
    for name in ('validar.py', 'validar-precompilado.py'):
        path = dest / name
        text = path.read_text().replace(old_most, most[0]).replace(old_fetch, fetch[0])
        if name == 'validar.py':
            text = text.replace(r'\(<2 x float>', r'\(' + first)
        path.write_text(text)
    for script in ('validar.py', 'validar-precompilado.py'):
        command = [sys.executable, str(dest / script), '--nsc', args.nsc, '--clang-root', args.clang_root]
        result = subprocess.run(command, env=os.environ.copy(), stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, text=True, errors='replace')
        (root / (label + '-' + script + '.log')).write_text(
            'COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
        print(label, script, 'EXIT', result.returncode, flush=True)
        if result.returncode:
            print(result.stdout, flush=True)
            raise SystemExit('Fallo; evidencia: ' + str(root))
print('OK: dos ordenes; evidencia:', root)
