"""Comprueba preserve_none con un record que contiene int[2][3]."""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
args = parser.parse_args()
fixture = Path(__file__).resolve().parent
root = Path(tempfile.mkdtemp(prefix='nsc-t09-preserve-arreglo2d-', dir='A:/temp'))
dest = root / 'case'
shutil.copytree(fixture, dest)
print('Evidencia:', root, flush=True)
for name in ('api.cpp', 'Control.cpp', 'Main.ns', 'Producer.ns'):
    path = dest / name
    data = path.read_text(encoding='utf-8')
    data = (data.replace('values[0]', 'values[0][0]')
                .replace('values[1]', 'values[0][1]')
                .replace('values[2]', 'values[1][1]')
                .replace('v{{3,4,7},1.5}', 'v{{{3,4,0},{0,7,0}},1.5}'))
    path.write_text(data, encoding='utf-8')
header = dest / 'api.h'
data = header.read_text(encoding='utf-8')
if 'int values[3]' not in data:
    raise SystemExit('falta declaracion del arreglo en ' + str(header))
header.write_text(data.replace('int values[3]', 'int values[2][3]'), encoding='utf-8')
for script in ('validar.py', 'validar-precompilado.py'):
    command = [sys.executable, str(dest / script), '--nsc', args.nsc, '--clang-root', args.clang_root]
    result = subprocess.run(command, cwd=fixture.parents[1], stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    log = root / (script + '.log')
    log.write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
    if result.returncode:
        raise SystemExit(script + ': fallo; evidencia: ' + str(log))
    print(script, 'OK', flush=True)
print('OK: arreglo multidimensional; evidencia:', root)
