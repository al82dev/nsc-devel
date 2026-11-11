"""T09-003: contrasta una llamada indirecta SysV con el productor y Clang."""
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
root = Path(tempfile.mkdtemp(prefix='nsc-t09-sysv-indirecta-', dir=r'A:\temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)

def run(label, command):
    result = subprocess.run(command, cwd=repo, env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    (root / (label + '.log')).write_text('COMMAND: ' + repr(command) + '\nEXIT: ' +
        str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
    if result.returncode:
        print(result.stdout, flush=True)
        raise SystemExit(label + ': fallo; evidencia: ' + str(root))
    return result.stdout

if 'clang version 23.1.0' not in run('version', ['clang.exe', '--version']):
    raise SystemExit('Se requiere Clang 23.1.0')
for name in ('api.h', 'api.c', 'Control.c', 'Main.ns', 'Static.nsproj', 'Bad.h', 'Bad.ns', 'Bad.nsproj',
             'BadConversion.ns', 'BadConversion.nsproj'):
    shutil.copyfile(fixture / name, root / name)
flags = ['-target', 'x86_64-pc-windows-msvc', '-std=c11', '-O0']
run('productor', ['clang.exe', *flags, '-c', str(root/'api.c'), '-o', str(root/'api.obj')])
run('biblioteca', ['llvm-lib.exe', '/nologo', '/out:' + str(root/'api.lib'), str(root/'api.obj')])
run('control-ir', ['clang.exe', *flags, '-S', '-emit-llvm', str(root/'Control.c'), '-o', str(root/'control.ll')])
ir = (root/'control.ll').read_text(encoding='utf-8')
for result_type in ('i32', 'i64', 'double', 'float', 'ptr', 'void'):
    if not re.search(r'call\s+x86_64_sysvcc\s+'+result_type+r'\s+%[^\(]+\(', ir):
        raise SystemExit('Clang no emitio la convencion SysV indirecta para '+result_type+'; evidencia: '+str(root))
run('control-obj', ['clang.exe', *flags, '-c', str(root/'Control.c'), '-o', str(root/'control.obj')])
run('control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console', '/nodefaultlib',
    '/out:' + str(root/'control.exe'), str(root/'control.obj'), str(root/'api.lib')])
run('control-run', [str(root/'control.exe')])
print('Control C y convencion indirecta de Clang: OK', flush=True)
common = [str(root/'Static.nsproj'), '--cpp-clang-root=' + args.clang_root, '--library-reference-root', str(root)]
ns_ir = run('ns-ir', [args.nsc, '--dump-project-oop-llvm', *common])
if not re.search(r'call\s+x86_64_sysvcc\s+i32\s+%[^\(]+\(i32', ns_ir):
    raise SystemExit('NativeSharp no emitio la convencion SysV indirecta; evidencia: ' + str(root))
run('ns-build', [args.nsc, '--emit-project-oop-exe', *common, '--no-artifact-cache', '-O0', '-o', str(root/'ns.exe')])
run('ns-run', [str(root/'ns.exe')])
command = [args.nsc, '--dump-project-oop-llvm', str(root/'Bad.nsproj'),
           '--cpp-clang-root=' + args.clang_root, '--library-reference-root', str(root)]
result = subprocess.run(command, cwd=repo, env=env, stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT, text=True, errors='replace')
(root/'rechazo-firma.log').write_text('COMMAND: ' + repr(command) + '\nEXIT: ' +
    str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
if result.returncode == 0 or 'sysv_abi function pointer signature is not yet supported' not in result.stdout:
    raise SystemExit('La firma no verificada no fue rechazada correctamente; evidencia: ' + str(root))
command = [args.nsc, '--dump-project-oop-llvm', str(root/'BadConversion.nsproj'),
           '--cpp-clang-root=' + args.clang_root, '--library-reference-root', str(root)]
result = subprocess.run(command, cwd=repo, env=env, stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT, text=True, errors='replace')
(root/'rechazo-conversion.log').write_text('COMMAND: ' + repr(command) + '\nEXIT: ' +
    str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
if result.returncode == 0 or 'sysv_abi function pointer is not yet supported' not in result.stdout:
    raise SystemExit('La conversion incompatible no fue rechazada correctamente; evidencia: ' + str(root))
print('OK: llamada indirecta C/NativeSharp, estatica O0; evidencia:', root, flush=True)
