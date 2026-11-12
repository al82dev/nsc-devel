"""Contrasta punteros regcall a enteros de ocho bits en C y NativeSharp."""
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
fixture = Path(__file__).resolve().parent
root = Path(tempfile.mkdtemp(prefix='nsc-t09-regcall-byte-ptr-', dir=r'A:\temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)
for label, c_type, ns_type in (('con-signo', 'signed char', 'sbyte'),
                                ('sin-signo', 'unsigned char', 'byte')):
    case = root / label
    case.mkdir()
    for name in ('api.h', 'api-dll.h', 'api.c', 'Control.c', 'Main.ns', 'Producer.ns',
                 'Consumer.ns', 'VectorCall.nsproj', 'VectorCallDll.nsproj',
                 'validar.py', 'validar-precompilado.py'):
        shutil.copyfile(fixture / name, case / name)
    for name in ('api.h', 'api.c', 'Control.c'):
        path = case / name
        path.write_text(path.read_text(encoding='utf-8').replace('short', c_type), encoding='utf-8')
    for name in ('Main.ns', 'Producer.ns'):
        path = case / name
        path.write_text(path.read_text(encoding='utf-8').replace('short', ns_type), encoding='utf-8')
    for script in ('validar.py', 'validar-precompilado.py'):
        command = [sys.executable, str(case / script), '--nsc', args.nsc, '--clang-root', args.clang_root]
        p = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                           text=True, errors='replace')
        (case / (script + '.log')).write_text('EXIT: ' + str(p.returncode) + '\n' + p.stdout, encoding='utf-8')
        if p.returncode:
            print(p.stdout, flush=True)
            raise SystemExit(label + ': fallo; evidencia: ' + str(case))
        print(label + ' ' + script + ': OK', flush=True)
print('OK: punteros con y sin signo; evidencia:', root)
