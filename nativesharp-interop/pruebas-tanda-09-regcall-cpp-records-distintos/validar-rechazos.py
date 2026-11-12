"""Conserva restricciones de campos privados y valores C++ no triviales."""
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
root = Path(tempfile.mkdtemp(prefix='nsc-t09-cpp-distinct-rechazos-', dir=r'A:\temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)
for name in ('api.h', 'api.cpp', 'Probe.nsproj'):
    shutil.copyfile(fixture/name, root/name)
for label, command in (
    ('compile', ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20',
                 '-c', str(root/'api.cpp'), '-o', str(root/'api.obj')]),
    ('archive', ['llvm-lib.exe', '/nologo', '/out:'+str(root/'api.lib'), str(root/'api.obj')]),
):
    result = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
    (root/(label+'.log')).write_text('EXIT: '+str(result.returncode)+'\n'+result.stdout, encoding='utf-8')
    if result.returncode:
        raise SystemExit(label+': fallo; evidencia: '+str(root))
original = (root/'api.h').read_text(encoding='utf-8')
for label, declaration in (
    ('privado', 'struct Array { private: float values[3]; };'),
    ('no-trivial', 'struct Array { float values[3]; ~Array(); };'),
):
    (root/'api.h').write_text(original.replace('struct Array { float values[3]; };', declaration), encoding='utf-8')
    (root/'Main.ns').write_text('int Main() { T09HfaCpp.Array a; a.values[0]=(float)1; return 0; }', encoding='utf-8')
    command = [args.nsc, '--dump-project-oop-llvm', str(root/'Probe.nsproj'), '--cpp-clang-root='+args.clang_root,
               '--library-reference-root', str(root)]
    result = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
    (root/(label+'.log')).write_text('EXIT: '+str(result.returncode)+'\n'+result.stdout, encoding='utf-8')
    if result.returncode == 0 or "has no compatible member named 'values'" not in result.stdout:
        raise SystemExit(label+': no produjo el rechazo esperado; evidencia: '+str(root))
print('OK: campos privados y registros no triviales rechazados; evidencia:', root, flush=True)
