"""Contrasta índices aritméticos C con Clang y una plantilla precompilada."""
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
root = Path(tempfile.mkdtemp(prefix='nsc-t09-c-size-index-expression-', dir='A:/temp'))
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
checks = ('    int index = t09_index();\n'
          '    if (t09_s1(&a.values[index+0]) != -1 || t09_s3(&a.values[index+0]) != 0) return 20;\n'
          '    if (t09_d1(&a.values[index+0]) != 16 || t09_d3(&a.values[index+0]) != 0) return 21;\n')
for name in ('Main.ns', 'Producer.ns', 'Control.c'):
    replace(name, '    return 0;', checks + '    return 0;')

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
print('OK: índices aritméticos C; evidencia:', root)
