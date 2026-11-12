"""Contrasta dos records expandidos regcall, con control C independiente."""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument('--nsc', required=True)
p.add_argument('--clang-root', required=True)
args = p.parse_args()
src = Path(__file__).resolve().parent
root = Path(tempfile.mkdtemp(prefix='nsc-t09-two-records-', dir=r'A:\temp'))
env = dict(os.environ, TEMP=str(root), TMP=str(root))
print('Evidencia:', root, flush=True)
def run(name, command):
    result = subprocess.run(command, cwd=root, env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    (root/(name+'.log')).write_text('COMMAND: '+repr(command)+'\nEXIT: '+str(result.returncode)+'\n'+result.stdout, encoding='utf-8')
    if result.returncode: raise SystemExit(name+': fallo; evidencia: '+str(root/(name+'.log')))
    return result.stdout
if 'clang version 23.1.0' not in run('version', ['clang.exe', '--version']):
    raise SystemExit('Se requiere Clang 23.1.0')
for opt in range(4):
    case = root/('O'+str(opt))
    case.mkdir()
    for name in ('api.h', 'api.c', 'Control.c', 'Main.ns', 'Probe.nsproj'):
        shutil.copyfile(src/name, case/name)
    flags = ['-target', 'x86_64-pc-windows-msvc', '-std=c11', '-O'+str(opt)]
    prefix = 'O'+str(opt)+'-'
    run(prefix+'producer', ['clang.exe', *flags, '-c', str(case/'api.c'), '-o', str(case/'api.obj')])
    run(prefix+'archive', ['llvm-lib.exe', '/nologo', '/out:'+str(case/'api.lib'), str(case/'api.obj')])
    run(prefix+'control-object', ['clang.exe', *flags, '-c', str(case/'Control.c'), '-o', str(case/'control.obj')])
    run(prefix+'control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console', '/nodefaultlib',
                         '/out:'+str(case/'control.exe'), str(case/'control.obj'), str(case/'api.lib')])
    run(prefix+'control-run', [str(case/'control.exe')])
    llvm = run(prefix+'llvm', [args.nsc, '--dump-project-oop-llvm', str(case/'Probe.nsproj'),
               '--cpp-clang-root='+args.clang_root])
    pieces = r',\s*'.join([r'float\s+[^,)]+']*6) + r',\s*i32\s+[^,)]+'
    if not re.search(r'call\s+x86_regcallcc\s+\{[^\n]+?\}\s+@(?:"\\01)?__regcall3__combine\('+pieces+r'\)', llvm):
        raise SystemExit(prefix+'no se encontró la llamada física; evidencia: '+str(root))
    run(prefix+'ns-build', [args.nsc, '--emit-project-oop-exe', str(case/'Probe.nsproj'),
                     '--cpp-clang-root='+args.clang_root, '--no-artifact-cache', '-O'+str(opt), '-o', str(case/'out.exe')])
    run(prefix+'ns-run', [str(case/'out.exe')])
    print(prefix+'control C y NativeSharp correctos', flush=True)
print('OK: cuatro comparaciones estáticas', flush=True)
case = root/'identidad-distinta'
case.mkdir()
for name in ('api.h', 'Main.ns', 'Probe.nsproj'):
    shutil.copyfile(src/name, case/name)
shutil.copyfile(root/'O0'/'api.lib', case/'api.lib')
header = (case/'api.h').read_text(encoding='utf-8')
header = header.replace('Triple __attribute__((regcall)) combine(Triple a, Triple b, int bias);',
    'typedef struct Other { Pair pair; float z; } Other;\n'
    'Triple __attribute__((regcall)) combine(Triple a, Other b, int bias);')
(case/'api.h').write_text(header, encoding='utf-8')
main = (case/'Main.ns').read_text(encoding='utf-8').replace('Triple b;', 'Other b;')
(case/'Main.ns').write_text(main, encoding='utf-8')
command = [args.nsc, '--dump-project-oop-llvm', str(case/'Probe.nsproj'), '--cpp-clang-root='+args.clang_root]
result = subprocess.run(command, cwd=root, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
(root/'identidad-distinta.log').write_text('COMMAND: '+repr(command)+'\nEXIT: '+str(result.returncode)+'\n'+result.stdout, encoding='utf-8')
if result.returncode == 0 or 'regcall currently supports' not in result.stdout:
    raise SystemExit('la identidad distinta no se rechazó como se esperaba; evidencia: '+str(root))
print('OK: identidad de record distinta rechazada', flush=True)
