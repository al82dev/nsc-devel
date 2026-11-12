"""Verifica el rechazo de una firma regcall agregada aun no admitida."""
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
root = Path(tempfile.mkdtemp(prefix='nsc-t09-regcall-negativo-', dir=r'A:\temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)
for name in ('api.h', 'api.c', 'Main.ns', 'VectorCall.nsproj'):
    shutil.copyfile(fixture / name, root / name)
for name in ('api.h', 'api.c'):
    path = root / name
    text = path.read_text(encoding='utf-8').replace('int a, int b, int c', 'Unsupported a, int b, int c')
    if name == 'api.h':
        text = text.replace('#pragma once', '#pragma once\ntypedef struct Unsupported { float x,y,z; } Unsupported;')
    else:
        text = text.replace('a * 100', '(int)a.x * 100')
    path.write_text(text, encoding='utf-8')
(root / 'Main.ns').write_text('int Main() { Unsupported value; value.x=(float)3; value.y=(float)0; value.z=(float)0; return t09_regcall(value,4,7)==347 ? 0 : 1; }\n', encoding='utf-8')
for label, command in (
    ('compile', ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-c', str(root / 'api.c'), '-o', str(root / 'api.obj')]),
    ('archive', ['llvm-lib.exe', '/nologo', '/out:' + str(root / 'api.lib'), str(root / 'api.obj')]),
):
    result = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
    (root / (label + '.log')).write_text('EXIT: ' + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
    if result.returncode:
        raise SystemExit(label + ': fallo; evidencia: ' + str(root))
command = [args.nsc, '--dump-project-oop-llvm', str(root / 'VectorCall.nsproj'),
           '--cpp-clang-root=' + args.clang_root, '--library-reference-root', str(root)]
result = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
(root / 'nsc.log').write_text('EXIT: ' + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
if result.returncode == 0 or 'regcall currently supports only int scalar signatures' not in result.stdout:
    raise SystemExit('La firma no verificada no fue rechazada como se esperaba; evidencia: ' + str(root))
print('OK: firma no verificada rechazada; evidencia:', root)
