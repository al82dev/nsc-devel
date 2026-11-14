"""Compara índices aritméticos con Clang, en directo y desde plantilla precompilada."""
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
root = Path(tempfile.mkdtemp(prefix='nsc-t09-size-index-expression-', dir='A:/temp'))
fixture = root / 'fixture'
shutil.copytree(source, fixture)

def replace(name, old, new):
    path = fixture / name
    text = path.read_text(encoding='utf-8')
    if text.count(old) != 1:
        raise SystemExit('control no encontrado: ' + name + ': ' + old)
    path.write_text(text.replace(old, new), encoding='utf-8')

replace('validar.py', "for name in ('size0', 'size1', 'size2', 'size3', 'dynamic0', 'dynamic1', 'dynamic2', 'dynamic3'):",
        "for name in ('size1', 'size3', 'dynamic1', 'dynamic3'):")
replace('validar.py', "for variant in ('call', 'local'):", "for variant in ('suma', 'resta', 'anidado'):")
replace('validar.py', "cpp_index = 'T09ObjectSize::index()' if variant == 'call' else 'index'",
        "cpp_index = {'suma':'index+0', 'resta':'index-1', 'anidado':'(index+1)-1'}[variant]")
replace('validar.py', "ns_index = 'T09ObjectSize.index()' if variant == 'call' else 'index'",
        "ns_index = {'suma':'index+0', 'resta':'index-1', 'anidado':'(index+1)-1'}[variant]")
replace('validar.py', "cpp_init = '' if variant == 'call' else 'int index=T09ObjectSize::index(); '",
        "cpp_init = 'int index=T09ObjectSize::index(); '")
replace('validar.py', "ns_init = '' if variant == 'call' else 'int index=T09ObjectSize.index(); '",
        "ns_init = 'int index=T09ObjectSize.index(); '")
path = fixture / 'Producer.ns'
text = path.read_text(encoding='utf-8')
if text.count('[runtimeIndex]') != 4:
    raise SystemExit('no se encontraron los índices de la plantilla')
path.write_text(text.replace('[runtimeIndex]', '[runtimeIndex+0]'), encoding='utf-8')

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
print('OK: índices aritméticos; evidencia:', root)
