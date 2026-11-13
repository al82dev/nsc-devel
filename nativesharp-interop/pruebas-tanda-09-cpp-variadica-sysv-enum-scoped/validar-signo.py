"""Comprueba enum class SysV signed short, llamada directa y callback."""
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
root = Path(tempfile.mkdtemp(prefix='nsc-t09-sysv-enum-signo-', dir='A:/temp'))
print('Evidencia:', root, flush=True)
for route, source in [('directa', fixture), ('indirecta', fixture.parent / (fixture.name + '-indirecta'))]:
    dest = root / route
    shutil.copytree(source, dest)
    for name in ('api.h', 'api.cpp', 'validar.py'):
        path = dest / name
        data = path.read_text(encoding='utf-8')
        data = (data.replace('Mode : unsigned short', 'Mode : short')
                    .replace('Chosen=65535', 'Chosen=-30000')
                    .replace('cmpw $-1, %si', 'cmpw $-30000, %si')
                    .replace('i16 zeroext', 'i16 signext'))
        path.write_text(data, encoding='utf-8')
    for script in ('validar.py', 'validar-precompilado.py'):
        command = [sys.executable, str(dest / script), '--nsc', args.nsc, '--clang-root', args.clang_root]
        result = subprocess.run(command, cwd=fixture.parents[1], stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, text=True, errors='replace')
        log = root / (route + '-' + script + '.log')
        log.write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
        if result.returncode:
            raise SystemExit(route + ' ' + script + ': fallo; evidencia: ' + str(log))
        print(route, script, 'OK', flush=True)
print('OK: enum signed short directo e indirecto; evidencia:', root)
