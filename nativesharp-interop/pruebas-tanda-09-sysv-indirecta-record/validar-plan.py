"""Valida la consistencia de un plan fijo indirecto y sus alteraciones."""
import os
from pathlib import Path
import subprocess
import tempfile

fixture = Path(__file__).resolve().parent
repo = fixture.parents[1]
root = Path(tempfile.mkdtemp(prefix='nsc-t09-pointer-plan-', dir=r'A:\temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)

def run(label, command):
    result = subprocess.run(command, cwd=repo, env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    (root / (label + '.log')).write_text('COMMAND: ' + repr(command) + '\nEXIT: ' +
        str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
    if result.returncode:
        print(result.stdout, flush=True)
        raise SystemExit(label + ': fallo; evidencia: ' + str(root))
    return result.stdout

if 'clang version 23.1.0' not in run('version', ['clang++.exe', '--version']):
    raise SystemExit('Se requiere Clang 23.1.0')
run('build', ['clang++.exe', '-std=c++20', '-I', str(repo / 'include'),
              str(fixture / 'validar-plan.cpp'), '-o', str(root / 'validate.exe')])
run('execute', [str(root / 'validate.exe')])
print('OK: plan valido y seis alteraciones; evidencia:', root, flush=True)
