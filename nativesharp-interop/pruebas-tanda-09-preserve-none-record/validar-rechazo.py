"""Record C++ preserve_none de 44 bytes con argumento extra, antes control negativo, comparado ahora; y rechazo separado.

La firma Small(Small, int) ya se admite por el plan de Clang (retorno indirecto y escalar adicional): la
prueba la compila con Clang, la ejecuta desde NativeSharp en directo y por puntero, y exige los valores.
El rechazo se conserva con una firma preserve_none de 33 parametros, fuera de los validadores compartidos.
"""
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
h.write_text(h.read_text(encoding='utf-8').replace('int x,y;', 'int x,y,z,w,v,q,r,a,b,c,d;')
    .replace('*)(Small);', '*)(Small,int);').replace('most(Small);', 'most(Small,int);'), encoding='utf-8')
implementation = root / 'api.cpp'
implementation.write_text(implementation.read_text(encoding='utf-8').replace('most(Small p)', 'most(Small p,int)'), encoding='utf-8')
main = root / 'Main.ns'
main.write_text(main.read_text(encoding='utf-8').replace('most(s)', 'most(s,1)').replace('callback(s)', 'callback(s,1)'), encoding='utf-8')

def run(name, command):
    result = subprocess.run(command, cwd=root, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
    (root / (name + '.log')).write_text(repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
    return result

if run('clang', ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-c', 'api.cpp', '-o', 'api.obj']).returncode:
    raise SystemExit('Clang fallo; evidencia: ' + str(root))
if run('lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj']).returncode:
    raise SystemExit('llvm-lib fallo; evidencia: ' + str(root))
result = run('nsc', [args.nsc, '--emit-project-oop-exe', 'VectorCall.nsproj', '--library-reference-root', str(root), '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-o', 'native.exe'])
if result.returncode:
    raise SystemExit('la firma admitida no compilo: ' + result.stdout + '\n' + str(root))
if run('native', [str(root / 'native.exe')]).returncode:
    raise SystemExit('la firma admitida devolvio valores distintos; evidencia: ' + str(root))
negative = root / 'treinta-y-tres'
negative.mkdir()
for name in ('VectorCall.nsproj',):
    shutil.copyfile(src / name, negative / name)
many = ', '.join(f'int a{i}' for i in range(33))
(negative / 'api.h').write_text('#pragma once\nnamespace T09Cpp {\nint __attribute__((preserve_none)) many(' + many + ');\n}\n',
    encoding='utf-8')
(negative / 'api.cpp').write_text('#include "api.h"\nnamespace T09Cpp {\nint __attribute__((preserve_none)) many(' + many +
    ') { return a0 + a32; }\n}\n', encoding='utf-8')
(negative / 'Main.ns').write_text('int Main() { return T09Cpp.many(' + ', '.join(str(i) for i in range(33)) + ') == 32 ? 0 : 1; }\n',
    encoding='utf-8')
def run_in(name, command):
    result = subprocess.run(command, cwd=negative, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
    (root / (name + '.log')).write_text(repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
    return result
if run_in('clang-33', ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-c', 'api.cpp', '-o', 'api.obj']).returncode:
    raise SystemExit('Clang fallo con 33 parametros; evidencia: ' + str(root))
if run_in('lib-33', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj']).returncode:
    raise SystemExit('llvm-lib fallo con 33 parametros; evidencia: ' + str(root))
result = run_in('nsc-33', [args.nsc, '--emit-project-oop-exe', 'VectorCall.nsproj', '--library-reference-root', str(negative), '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-o', 'native.exe'])
if result.returncode == 0 or ('preserve_none' not in result.stdout and 'unsupported' not in result.stdout):
    raise SystemExit('rechazo inesperado: ' + result.stdout + '\n' + str(root))
print('OK: firma con argumento adicional comparada y firma de 33 parametros rechazada; evidencia:', root)
