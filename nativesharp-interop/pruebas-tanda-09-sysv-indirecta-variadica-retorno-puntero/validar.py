"""Contraste C/NS de un record por valor en un puntero a funcion SysV."""
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
fixture = Path(__file__).resolve().parent
repo = fixture.parents[1]
root = Path(tempfile.mkdtemp(prefix='nsc-t09-sysv-indirecta-variadica-retorno-puntero-', dir=r'A:\temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)

def run(label, command):
    result = subprocess.run(command, cwd=repo, env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    (root/(label+'.log')).write_text('COMMAND: '+repr(command)+'\nEXIT: '+
        str(result.returncode)+'\n'+result.stdout, encoding='utf-8')
    if result.returncode:
        print(result.stdout, flush=True)
        raise SystemExit(label+': fallo; evidencia: '+str(root))
    return result.stdout

if 'clang version 23.1.0' not in run('version', ['clang.exe', '--version']):
    raise SystemExit('Se requiere Clang 23.1.0')
for name in ('api.h', 'api.c', 'producer.s', 'Control.c', 'Main.ns', 'Static.nsproj'):
    shutil.copyfile(fixture/name, root/name)
flags = ['-target', 'x86_64-pc-windows-msvc', '-std=c11', '-O0']
run('producer', ['clang.exe', *flags, '-c', str(root/'api.c'), '-o', str(root/'api.obj')])
run('asm', ['clang.exe', *flags, '-c', str(root/'producer.s'), '-o', str(root/'producer.obj')])
run('library', ['llvm-lib.exe', '/nologo', '/out:'+str(root/'api.lib'), str(root/'api.obj'), str(root/'producer.obj')])
run('control-ir', ['clang.exe', *flags, '-S', '-emit-llvm', str(root/'Control.c'), '-o', str(root/'control.ll')])
pattern = r'call\s+x86_64_sysvcc\s+ptr\s+\(ptr,\s*\.\.\.\)\s+%[^\(]+\(ptr\s+[^,]+,\s*double\s+[^,]+,\s*i32\s+[^,]+,\s*i32\s+'
if not re.search(pattern, (root/'control.ll').read_text(encoding='utf-8')):
    raise SystemExit('Clang no emitio las piezas esperadas; evidencia: '+str(root))
run('control-obj', ['clang.exe', *flags, '-c', str(root/'Control.c'), '-o', str(root/'control.obj')])
run('control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console', '/nodefaultlib',
    '/out:'+str(root/'control.exe'), str(root/'control.obj'), str(root/'api.lib')])
run('control-run', [str(root/'control.exe')])
print('Control C y piezas de Clang: OK', flush=True)
common = [str(root/'Static.nsproj'), '--cpp-clang-root='+args.clang_root, '--library-reference-root', str(root)]
ir = run('ns-ir', [args.nsc, '--dump-project-oop-llvm', *common])
if not re.search(pattern, ir):
    raise SystemExit('NS no emitio las piezas esperadas; evidencia: '+str(root))
run('ns-build', [args.nsc, '--emit-project-oop-exe', *common, '--no-artifact-cache', '-O0', '-o', str(root/'ns.exe')])
run('ns-run', [str(root/'ns.exe')])
print('OK: record indirecto C/NS; evidencia:', root, flush=True)
