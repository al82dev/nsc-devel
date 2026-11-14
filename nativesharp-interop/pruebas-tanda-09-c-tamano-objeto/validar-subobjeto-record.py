"""Compara tamaños de structs y unions C locales y anidados con Clang."""
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
root = Path(tempfile.mkdtemp(prefix='nsc-t09-c-size-record-', dir='A:/temp'))
fixture = root / 'fixture'
shutil.copytree(source, fixture)

def replace(name, old, new):
    path = fixture / name
    text = path.read_text(encoding='utf-8')
    if text.count(old) != 1:
        raise SystemExit('control no encontrado: ' + name)
    path.write_text(text.replace(old, new), encoding='utf-8')

types = ('struct T09Record { char a; double b; };\n'
         'struct T09RecordOuter { struct T09Record inner; int tail; };\n'
         'union T09Union { int a; double b; };\n'
         'struct T09UnionOuter { union T09Union inner; int tail; };\n'
         'struct T09PointerOuter { struct T09Record *inner; int tail; };\n')
declarations, definitions, ns_checks, c_checks = [], [], [], []
for dynamic in (False, True):
    for mode in range(4):
        attr = 'T09_DYNAMIC' if dynamic else 'T09_SIZE'
        builtin = '__builtin_dynamic_object_size' if dynamic else '__builtin_object_size'
        for tag, label, size, outer in (('struct', 'Record', 16, 24), ('union', 'Union', 8, 16)):
            name = 't09_record_' + ('d' if dynamic else 's') + str(mode) + label
            declarations.append(f'T09_STORAGE int {name}({tag} T09{label} *p {attr}({mode}));')
            definitions.append(f'int {name}({tag} T09{label} *const p {attr}({mode})) '
                               f'{{ return (int){builtin}(p, {mode}); }}')
            expected = size if mode in (1, 3) else outer
            ns_checks.append(f'    if ({name}(&v{label}) != {size} || {name}(&o{label}.inner) != {expected}) return 40;')
            c_checks.append(ns_checks[-1])
        name = 't09_record_' + ('d' if dynamic else 's') + str(mode) + 'Pointer'
        declarations.append(f'T09_STORAGE int {name}(struct T09Record **p {attr}({mode}));')
        definitions.append(f'int {name}(struct T09Record **const p {attr}({mode})) '
                           f'{{ return (int){builtin}(p, {mode}); }}')
        expected = 8 if mode in (1, 3) else 16
        ns_checks.append(f'    if ({name}(&pointerLocal) != 8 || {name}(&pointerOuter.inner) != {expected}) return 41;')
        c_checks.append(ns_checks[-1])
replace('api.h', 'struct T09CArray', types + '\n'.join(declarations) + '\nstruct T09CArray')
path = fixture / 'api.c'
path.write_text(path.read_text(encoding='utf-8') + '\n' + '\n'.join(definitions) + '\n', encoding='utf-8')
for name in ('Main.ns', 'Producer.ns'):
    replace(name, '    return 0;', '    T09Record vRecord; T09RecordOuter oRecord;\n'
            '    T09Union vUnion; T09UnionOuter oUnion;\n'
            '    T09Record* pointerLocal = null; T09PointerOuter pointerOuter;\n'
            + '\n'.join(ns_checks) + '\n    return 0;')
replace('Control.c', '    return 0;', '    struct T09Record vRecord; struct T09RecordOuter oRecord;\n'
        '    union T09Union vUnion; struct T09UnionOuter oUnion;\n'
        '    struct T09Record* pointerLocal = 0; struct T09PointerOuter pointerOuter;\n'
        + '\n'.join(c_checks) + '\n    return 0;')
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
print('OK: tamaños de structs y unions C; evidencia:', root)
