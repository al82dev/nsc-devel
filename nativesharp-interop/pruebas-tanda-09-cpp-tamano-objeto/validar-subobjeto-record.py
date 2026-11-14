"""Compara tamaños de records locales y campos con Clang, también precompilados."""
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
root = Path(tempfile.mkdtemp(prefix='nsc-t09-size-record-', dir='A:/temp'))
fixture = root / 'fixture'
shutil.copytree(source, fixture)

def replace(name, old, new):
    path = fixture / name
    text = path.read_text(encoding='utf-8')
    if text.count(old) != 1:
        raise SystemExit('control no encontrado: ' + name)
    path.write_text(text.replace(old, new), encoding='utf-8')

replace('api.h', 'struct Pair { int first; int second; };',
        'struct Pair { int first; int second; };\nstruct Outer { Pair inner; int tail; };\n'
        'struct Padded { char first; double second; };\nstruct OuterPadded { Padded inner; int tail; };\n'
        'struct OuterPointer { Pair *inner; int tail; };')
declarations = []
definitions = []
for dynamic in (False, True):
    for mode in range(4):
        name = ('record_dynamic' if dynamic else 'record_size') + str(mode)
        attribute = 'pass_dynamic_object_size' if dynamic else 'pass_object_size'
        builtin = '__builtin_dynamic_object_size' if dynamic else '__builtin_object_size'
        for record, suffix in (('Pair', ''), ('Padded', '_padded')):
            declarations.append(f'T09_STORAGE int {name}{suffix}({record} *p __attribute__(({attribute}({mode}))));')
            definitions.append(f'int T09ObjectSize::{name}{suffix}({record} *const p __attribute__(({attribute}({mode})))) '
                               f'{{ return static_cast<int>({builtin}(p, {mode})); }}')
        declarations.append(f'T09_STORAGE int {name}_pointer(Pair **p __attribute__(({attribute}({mode}))));')
        definitions.append(f'int T09ObjectSize::{name}_pointer(Pair **const p __attribute__(({attribute}({mode})))) '
                           f'{{ return static_cast<int>({builtin}(p, {mode})); }}')
replace('api.h', 'T09_STORAGE int calls();', '\n'.join(declarations) + '\nT09_STORAGE int calls();')
path = fixture / 'api.cpp'
path.write_text(path.read_text(encoding='utf-8') + '\n' + '\n'.join(definitions) + '\n', encoding='utf-8')

ns_tests = []
cpp_tests = []
for dynamic in (False, True):
    for mode in range(4):
        name = ('record_dynamic' if dynamic else 'record_size') + str(mode)
        expected = 8 if mode in (1, 3) else 12
        ns_tests.append(f'    if (T09ObjectSize.{name}(&recordLocal) != 8 || '
                        f'T09ObjectSize.{name}(&outer.inner) != {expected}) return 40;')
        cpp_tests.append(f'    if (T09ObjectSize::{name}(&recordLocal) != 8 || '
                         f'T09ObjectSize::{name}(&outer.inner) != {expected}) return 40;')
        padded_expected = 16 if mode in (1, 3) else 24
        ns_tests.append(f'    if (T09ObjectSize.{name}_padded(&paddedLocal) != 16 || '
                        f'T09ObjectSize.{name}_padded(&paddedOuter.inner) != {padded_expected}) return 41;')
        cpp_tests.append(f'    if (T09ObjectSize::{name}_padded(&paddedLocal) != 16 || '
                         f'T09ObjectSize::{name}_padded(&paddedOuter.inner) != {padded_expected}) return 41;')
        pointer_expected = 8 if mode in (1, 3) else 16
        ns_tests.append(f'    if (T09ObjectSize.{name}_pointer(&pointerLocal) != 8 || '
                        f'T09ObjectSize.{name}_pointer(&pointerOuter.inner) != {pointer_expected}) return 42;')
        cpp_tests.append(f'    if (T09ObjectSize::{name}_pointer(&pointerLocal) != 8 || '
                         f'T09ObjectSize::{name}_pointer(&pointerOuter.inner) != {pointer_expected}) return 42;')
for name in ('Main.ns', 'Producer.ns'):
    replace(name, '    return 0;', '    T09ObjectSize.Pair recordLocal; T09ObjectSize.Outer outer;\n'
            '    T09ObjectSize.Padded paddedLocal; T09ObjectSize.OuterPadded paddedOuter;\n'
            '    T09ObjectSize.Pair* pointerLocal = null; T09ObjectSize.OuterPointer pointerOuter;\n'
            + '\n'.join(ns_tests) + '\n    return 0;')
replace('Control.cpp', '    return T09ObjectSize::size0',
        '    T09ObjectSize::Pair recordLocal; T09ObjectSize::Outer outer;\n'
        '    T09ObjectSize::Padded paddedLocal; T09ObjectSize::OuterPadded paddedOuter;\n'
        '    T09ObjectSize::Pair* pointerLocal = nullptr; T09ObjectSize::OuterPointer pointerOuter;\n'
        + '\n'.join(cpp_tests) + '\n    return T09ObjectSize::size0')

# Los índices variables ya tienen su propia batería; aquí se prueba el record.
path = fixture / 'validar.py'
text = path.read_text(encoding='utf-8')
start = text.index('    if not args.solo_extremos:\n        for name in ')
end = text.index('    if opt == 0:', start)
path.write_text(text[:start] + text[end:], encoding='utf-8')

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
print('OK: tamaño de records locales y campos; evidencia:', root)
