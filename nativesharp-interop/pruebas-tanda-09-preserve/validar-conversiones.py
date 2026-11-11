"""Rechaza conversiones sin soporte de preserve_most/all a cdecl."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
args = parser.parse_args()
fixture = Path(__file__).resolve().parent
root = Path(tempfile.mkdtemp(prefix='nsc-t09-preserve-conversiones-', dir=r'A:\temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)
for name in ('api.h', 'api.c', 'VectorCall.nsproj'):
    shutil.copyfile(fixture / name, root / name)
with (root / 'api.h').open('a', encoding='utf-8') as header:
    header.write('\ntypedef int (*T09CdeclCallback)(int,int,int);\n')
for label, command in (
    ('compile', ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-c', str(root / 'api.c'), '-o', str(root / 'api.obj')]),
    ('archive', ['llvm-lib.exe', '/nologo', '/out:' + str(root / 'api.lib'), str(root / 'api.obj')]),
):
    result = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
    (root / (label + '.log')).write_text('EXIT: ' + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
    if result.returncode: raise SystemExit(label + ': fallo; evidencia: ' + str(root))
for function in ('t09_most', 't09_all'):
    (root / 'Main.ns').write_text('int Main() { T09CdeclCallback p = ' + function + '; return p(1,2,3); }', encoding='utf-8')
    command = [args.nsc, '--dump-project-oop-llvm', str(root / 'VectorCall.nsproj'),
               '--cpp-clang-root=' + args.clang_root, '--library-reference-root', str(root)]
    result = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
    (root / (function + '.log')).write_text('EXIT: ' + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
    if result.returncode == 0 or 'conversion of a preserve convention function' not in result.stdout:
        raise SystemExit(function + ': no produjo el rechazo esperado; evidencia: ' + str(root))
print('OK: conversiones incompatibles rechazadas; evidencia:', root)
