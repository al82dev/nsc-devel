"""La firma mixta con registro por valor no se admite sin el alias que aporta el plan ABI."""
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
source = Path(__file__).resolve().parent
root = Path(tempfile.mkdtemp(prefix='nsc-t09-preserve-none-retorno-record-padding-sin-plan-', dir=r'A:\temp'))
for name in ('api.h', 'api.cpp', 'Main.ns', 'VectorCall.nsproj'):
    shutil.copyfile(source / name, root / name)
main = root / 'Main.ns'
main.write_text(main.read_text(encoding='utf-8').replace('T09Cpp.Callback callback=', 'var callback='), encoding='utf-8')
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)

def run(name, command):
    result = subprocess.run(command, cwd=root, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
    (root / (name + '.log')).write_text(repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
    return result

if run('clang', ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-c', 'api.cpp', '-o', 'api.obj']).returncode:
    raise SystemExit('Clang fallo; evidencia: ' + str(root))
if run('lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj']).returncode:
    raise SystemExit('llvm-lib fallo; evidencia: ' + str(root))
result = run('nsc', [args.nsc, '--emit-project-oop-exe', str(root / 'VectorCall.nsproj'),
    '--library-reference-root', str(root), '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-o', str(root / 'test.exe')])
if result.returncode == 0 or 'preserve_none function pointer signature is not yet supported' not in result.stdout:
    raise SystemExit('rechazo inesperado: ' + result.stdout + '\n' + str(root))
print('OK: sin plan conciliado la llamada se rechaza; evidencia:', root)
