"""Compara un registro de double regcall con un control C de Clang."""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
args = parser.parse_args()
src = Path(__file__).resolve().parent
root = Path(tempfile.mkdtemp(prefix='nsc-t09-regcall-retorno-double-record-', dir=r'A:\temp'))
env = dict(os.environ, TEMP=str(root), TMP=str(root))
print('Evidencia:', root, flush=True)
for name in ('api.h', 'api.c', 'Control.c', 'Main.ns', 'Probe.nsproj'):
    shutil.copyfile(src/name, root/name)

def run(name, command):
    result = subprocess.run(command, cwd=root, env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    log = root/(name+'.log')
    log.write_text('COMMAND: '+repr(command)+'\nEXIT: '+str(result.returncode)+'\n'+result.stdout, encoding='utf-8')
    if result.returncode:
        raise SystemExit(name+': fallo; evidencia: '+str(log))
    return result.stdout

if 'clang version 23.1.0' not in run('version', ['clang.exe', '--version']):
    raise SystemExit('Se requiere Clang 23.1.0')
flags = ['-target', 'x86_64-pc-windows-msvc', '-std=c11', '-O0']
run('clang-llvm', ['clang.exe', *flags, '-S', '-emit-llvm', 'api.c', '-o', 'api.ll'])
run('producer', ['clang.exe', *flags, '-c', 'api.c', '-o', 'api.obj'])
run('archive', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'])
run('control-object', ['clang.exe', *flags, '-c', 'Control.c', '-o', 'control.obj'])
run('control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console', '/nodefaultlib',
                     '/out:control.exe', 'control.obj', 'api.lib'])
run('control-run', [str(root/'control.exe')])
ir = run('llvm', [args.nsc, '--dump-project-oop-llvm', str(root/'Probe.nsproj'),
                  '--cpp-clang-root='+args.clang_root])
arguments = r',\s*'.join([r'float\s+[^,)]+']*3) + r',\s*i32\s+[^,)]+'
result = r'\{\s*double,\s*double\s*\}'
for target in (r'@(?:"\\01)?__regcall3__combine', r'%[^\s(]+'):
    if not re.search(r'call\s+x86_regcallcc\s+'+result+r'\s+'+target+r'\('+arguments+r'\)', ir):
        raise SystemExit('Falta la llamada física; evidencia: '+str(root/'llvm.log'))
for opt in range(4):
    output = root/('out-O'+str(opt)+'.exe')
    run('nsc-O'+str(opt), [args.nsc, '--emit-project-oop-exe', str(root/'Probe.nsproj'),
        '--cpp-clang-root='+args.clang_root, '--no-artifact-cache', '-O'+str(opt), '-o', str(output)])
    run('nsc-run-O'+str(opt), [str(output)])
print('OK: control C y NativeSharp O0-O3, directo y por puntero; evidencia:', root, flush=True)

