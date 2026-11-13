"""Rechaza la conversión entre punteros con convenciones diferentes."""
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
root = Path(tempfile.mkdtemp(prefix='nsc-t09-preserve-none-callback-negativo-', dir=r'A:\temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
for name in ('api.h', 'api.cpp', 'VectorCall.nsproj'):
    shutil.copyfile(src / name, root / name)
with (root / 'api.h').open('a', encoding='utf-8') as f:
    f.write('\nnamespace T09Cpp { API_STORAGE double normal(double x, int* p, short s, bool b); }\n')
with (root / 'api.cpp').open('a', encoding='utf-8') as f:
    f.write('\nnamespace T09Cpp { double normal(double x, int* p, short s, bool b) { return x + *p + s + b; } }\n')
(root / 'Main.ns').write_text('int Main() { T09Cpp.Callback c = T09Cpp.normal; return 0; }\n', encoding='utf-8')

def run(name, command, valid):
    result = subprocess.run(command, cwd=root, env=env, text=True, errors='replace', stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (root / (name + '.log')).write_text(repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
    if not valid(result):
        raise SystemExit(name + ' fallo: ' + result.stdout + '\n' + str(root))

run('clang', ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-c', 'api.cpp', '-o', 'api.obj'], lambda r: r.returncode == 0)
run('lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], lambda r: r.returncode == 0)
run('nsc', [args.nsc, '--emit-project-oop-exe', 'VectorCall.nsproj', '--library-reference-root', str(root), '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-o', 'native.exe'], lambda r: r.returncode != 0 and 'conversion of a preserve convention function' in r.stdout)
print('OK: convencion incorrecta rechazada; evidencia:', root)
