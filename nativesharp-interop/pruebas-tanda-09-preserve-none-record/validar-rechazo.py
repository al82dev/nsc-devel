"""Rechaza el registro preserve_none de dieciséis bytes aún no implementado."""
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
src = Path(__file__).resolve().parent
root = Path(tempfile.mkdtemp(prefix='nsc-t09-preserve-none-record-negativo-', dir=r'A:\temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
for name in ('api.h', 'api.cpp', 'Main.ns', 'VectorCall.nsproj'):
    shutil.copyfile(src / name, root / name)
h = root / 'api.h'
h.write_text(h.read_text(encoding='utf-8').replace('int x,y;', 'int x,y,z,w;'), encoding='utf-8')

def run(name, command):
    result = subprocess.run(command, cwd=root, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
    (root / (name + '.log')).write_text(repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
    return result

if run('clang', ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-c', 'api.cpp', '-o', 'api.obj']).returncode:
    raise SystemExit('Clang fallo; evidencia: ' + str(root))
if run('lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj']).returncode:
    raise SystemExit('llvm-lib fallo; evidencia: ' + str(root))
result = run('nsc', [args.nsc, '--emit-project-oop-exe', 'VectorCall.nsproj', '--library-reference-root', str(root), '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-o', 'native.exe'])
if result.returncode == 0 or ('preserve_none' not in result.stdout and 'unsupported' not in result.stdout):
    raise SystemExit('rechazo inesperado: ' + result.stdout + '\n' + str(root))
print('OK: Clang acepta y NSC rechaza el registro de 16 bytes; evidencia:', root)
