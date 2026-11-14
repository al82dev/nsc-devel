"""Transporte HIR/MIR y emision LLVM de un record variadico por un puntero a funcion SysV."""
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
root = Path(tempfile.mkdtemp(prefix='nsc-t09-sysv-indirecta-variadica-record-', dir=r'A:\temp'))
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
pattern = r'call\s+x86_64_sysvcc\s+i32\s+\(i32,\s*\.\.\.\)\s+%[^\(]+\(i32\s+[^,]+,\s*double\s+[^,]+,\s*i32\s+'
if not re.search(pattern, (root/'control.ll').read_text(encoding='utf-8')):
    raise SystemExit('Clang no emitio las piezas esperadas; evidencia: '+str(root))
run('control-obj', ['clang.exe', *flags, '-c', str(root/'Control.c'), '-o', str(root/'control.obj')])
run('control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console', '/nodefaultlib',
    '/out:'+str(root/'control.exe'), str(root/'control.obj'), str(root/'api.lib')])
run('control-run', [str(root/'control.exe')])
print('Control C y piezas de Clang: OK', flush=True)
common = [str(root/'Static.nsproj'), '--cpp-clang-root='+args.clang_root, '--library-reference-root', str(root)]
for stage in ('hir', 'mir'):
    for jobs in (1, 4):
        dump = run('ns-' + stage + '-j' + str(jobs),
            [args.nsc, '--dump-project-oop-' + stage, *common, '-j', str(jobs)])
        if 'cpp-variadic-plan=__nsc_variadic_pointer_abi_probe:2' not in dump:
            raise SystemExit('Falta el plan en ' + stage + '; evidencia: ' + str(root))
command = [args.nsc, '--dump-project-oop-llvm', *common]
result = subprocess.run(command, cwd=repo, env=env, stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT, text=True, errors='replace')
(root / 'ns-ir.log').write_text('COMMAND: ' + repr(command) + '\nEXIT: ' +
    str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
# EN: T09-003. The emitter already lowers this variadic record pointer call (the full comparison is
#     validar.py in this fixture), so the former LLVM barrier is replaced by the Clang pieces in the IR.
# ES: T09-003. El emisor ya baja esta llamada (la comparacion completa es validar.py de este fixture):
#     la barrera LLVM anterior se sustituye por las piezas de Clang en el IR.
if result.returncode or not re.search(pattern, result.stdout):
    raise SystemExit('NSC no emitio las piezas variadicas de Clang; evidencia: ' + str(root))
print('OK: transporte HIR/MIR y piezas LLVM de Clang; evidencia:', root, flush=True)
