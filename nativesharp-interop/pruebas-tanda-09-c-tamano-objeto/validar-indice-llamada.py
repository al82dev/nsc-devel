"""Contrasta llamadas anidadas en índices C y verifica evaluación única."""
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
root = Path(tempfile.mkdtemp(prefix='nsc-t09-c-size-index-call-', dir='A:/temp'))
fixture = root / 'fixture'
shutil.copytree(source, fixture)

def replace(name, old, new):
    path = fixture / name
    text = path.read_text(encoding='utf-8')
    if text.count(old) != 1:
        raise SystemExit('control no encontrado: ' + name)
    path.write_text(text.replace(old, new), encoding='utf-8')

path = fixture / 'api.h'
path.write_text(path.read_text(encoding='utf-8') + '\nT09_STORAGE int t09_index(void);\n'
                'T09_STORAGE int t09_index_calls(void);\n', encoding='utf-8')
path = fixture / 'api.c'
path.write_text(path.read_text(encoding='utf-8') + '\nstatic int t09_index_count;\n'
                'int t09_index(void) { ++t09_index_count; return 1; }\n'
                'int t09_index_calls(void) { return t09_index_count; }\n', encoding='utf-8')
checks = []
for count, name in enumerate(('t09_s0', 't09_s1', 't09_s2', 't09_s3', 't09_d0', 't09_d1', 't09_d2', 't09_d3'), 1):
    expected = '-1' if name.endswith(('0', '1')) else '0'
    checks.append(f'    if ({name}(&a.values[t09_index()+0]) != {expected} || t09_index_calls() != {count}) return 20;')
for name in ('Main.ns', 'Producer.ns', 'Control.c'):
    replace(name, '    return 0;', '\n'.join(checks) + '\n    return 0;')

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
print('OK: llamadas en índices C, evaluación única; evidencia:', root)
