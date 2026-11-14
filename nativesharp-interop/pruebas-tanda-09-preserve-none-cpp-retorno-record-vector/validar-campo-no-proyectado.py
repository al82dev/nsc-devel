"""Comprueba que un vector de tres elementos con relleno sigue sin proyectarse."""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
args = parser.parse_args()
source = Path(__file__).resolve().parent
root = Path(tempfile.mkdtemp(prefix='nsc-t09-vector-campo-oculto-', dir='A:/temp'))
for name in ('api.h', 'api.cpp', 'VectorCall.nsproj'):
    shutil.copyfile(source / name, root / name)
header = root / 'api.h'
data = header.read_text(encoding='utf-8')
if 'ext_vector_type(4)' not in data:
    raise SystemExit('falta el tipo vectorial de control')
header.write_text(data.replace('ext_vector_type(4)', 'ext_vector_type(3)'), encoding='utf-8')
(root / 'Main.ns').write_text('int Main() { T09Cpp.Small v; return (int)v.values[0]; }\n', encoding='utf-8')
def run(label, command):
    p = subprocess.run(command, cwd=root, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
    (root / (label + '.log')).write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(p.returncode) + '\n' + p.stdout, encoding='utf-8')
    return p
p = run('clang', ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-c', 'api.cpp', '-o', 'api.obj'])
if p.returncode:
    raise SystemExit('Clang fallo; evidencia: ' + str(root))
p = run('lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'])
if p.returncode:
    raise SystemExit('llvm-lib fallo; evidencia: ' + str(root))
p = run('nsc', [args.nsc, '--dump-project-oop-hir', str(root / 'VectorCall.nsproj'),
                '--library-reference-root', str(root), '--cpp-clang-root=' + args.clang_root])
if p.returncode == 0 or 'values' not in p.stdout:
    raise SystemExit('el acceso no se rechazo como se esperaba; evidencia: ' + str(root))
print('OK: el campo vectorial no proyectado se rechaza; evidencia:', root)
