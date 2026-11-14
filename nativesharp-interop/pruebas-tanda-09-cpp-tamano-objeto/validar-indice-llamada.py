"""Contrasta índices que llaman una función y verifica evaluación única."""
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
root = Path(tempfile.mkdtemp(prefix='nsc-t09-size-index-call-', dir='A:/temp'))
fixture = root / 'fixture'
shutil.copytree(source, fixture)

def replace(name, old, new):
    path = fixture / name
    text = path.read_text(encoding='utf-8')
    if text.count(old) != 1:
        raise SystemExit('control no encontrado: ' + name + ': ' + old)
    path.write_text(text.replace(old, new), encoding='utf-8')

replace('api.h', 'T09_STORAGE int index();', 'T09_STORAGE int index();\nT09_STORAGE int indexCalls();')
replace('api.cpp', 'int T09ObjectSize::index() { return 1; }',
        'namespace { int indexCount; }\nint T09ObjectSize::index() { ++indexCount; return 1; }\n'
        'int T09ObjectSize::indexCalls() { return indexCount; }')
replace('validar.py', "for name in ('size0', 'size1', 'size2', 'size3', 'dynamic0', 'dynamic1', 'dynamic2', 'dynamic3'):",
        "for name in ('size1', 'size3', 'dynamic1', 'dynamic3'):")
replace('validar.py', "for variant in ('call', 'local'):", "for variant in ('suma', 'anidado'):")
replace('validar.py', "cpp_index = 'T09ObjectSize::index()' if variant == 'call' else 'index'",
        "cpp_index = 'T09ObjectSize::index()+0' if variant == 'suma' else '(T09ObjectSize::index()+1)-1'")
replace('validar.py', "ns_index = 'T09ObjectSize.index()' if variant == 'call' else 'index'",
        "ns_index = 'T09ObjectSize.index()+0' if variant == 'suma' else '(T09ObjectSize.index()+1)-1'")
replace('validar.py', "cpp_init = '' if variant == 'call' else 'int index=T09ObjectSize::index(); '", "cpp_init = ''")
replace('validar.py', "ns_init = '' if variant == 'call' else 'int index=T09ObjectSize.index(); '", "ns_init = ''")
replace('validar.py', "'return T09ObjectSize::' + name + '(&a.values[' + cpp_index + ']); }\\n'",
        "'int result=T09ObjectSize::' + name + '(&a.values[' + cpp_index + ']); if (T09ObjectSize::indexCalls()!=1) return 77; return result; }\\n'")
replace('validar.py', "'return T09ObjectSize.' + name + '(&a.values[' + ns_index + ']); }\\n'",
        "'int result=T09ObjectSize.' + name + '(&a.values[' + ns_index + ']); if (T09ObjectSize.indexCalls()!=1) return 77; return result; }\\n'")
replace('validar.py', 'if outcomes[0] != outcomes[1]:',
        'if outcomes[0] == 77 or outcomes[1] == 77 or outcomes[0] != outcomes[1]:')

checks = []
for offset, name in enumerate(('size0', 'size1', 'size2', 'size3', 'dynamic0', 'dynamic1', 'dynamic2', 'dynamic3'), 2):
    expected = '-1' if name.endswith(('0', '1')) else '0'
    checks.append(f'    if (T09ObjectSize.{name}(&array.values[T09ObjectSize.index()+0]) != {expected} '
                  f'|| T09ObjectSize.indexCalls() != {offset}) return 30;')
replace('Producer.ns', '    return 0;', '\n'.join(checks) + '\n    return 0;')

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
print('OK: llamadas en índices, evaluación única; evidencia:', root)
