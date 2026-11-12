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
p.add_argument('--indirecta', action='store_true')
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
for mode in ('estatica', 'dll'):
  for opt in range(4):
    case = root/(mode+'-O'+str(opt))
    case.mkdir()
    for name in ('api.h', 'api-dll.h', 'api.c', 'Control.c', 'Main.ns', 'Probe.nsproj', 'ProbeDll.nsproj'):
        shutil.copyfile(src/name, case/name)
    if args.indirecta:
        shutil.copyfile(src/'MainIndirect.ns', case/'Main.ns')
    flags = ['-target', 'x86_64-pc-windows-msvc', '-std=c11', '-O'+str(opt)]
    prefix = mode+'-O'+str(opt)+'-'
    producer_flags = ['-DAPI_DLL_BUILD'] if mode == 'dll' else []
    run(prefix+'producer', ['clang.exe', *flags, *producer_flags, '-c', str(case/'api.c'), '-o', str(case/'api.obj')])
    if mode == 'dll':
        library, project, control_flags = 'api-dll.lib', 'ProbeDll.nsproj', ['-DAPI_DLL_IMPORT']
        run(prefix+'dll', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib',
            '/export:__regcall3__combine', '/export:get_combine', '/out:'+str(case/'api.dll'),
            '/implib:'+str(case/library), str(case/'api.obj')])
    else:
        library, project, control_flags = 'api.lib', 'Probe.nsproj', []
        run(prefix+'archive', ['llvm-lib.exe', '/nologo', '/out:'+str(case/library), str(case/'api.obj')])
    run(prefix+'control-object', ['clang.exe', *flags, *control_flags, '-c', str(case/'Control.c'), '-o', str(case/'control.obj')])
    run(prefix+'control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console', '/nodefaultlib',
                         '/out:'+str(case/'control.exe'), str(case/'control.obj'), str(case/library)])
    run(prefix+'control-run', [str(case/'control.exe')])
    llvm = run(prefix+'llvm', [args.nsc, '--dump-project-oop-llvm', str(case/project),
               '--cpp-clang-root='+args.clang_root])
    pieces = r',\s*'.join([r'float\s+[^,)]+']*6) + r',\s*i32\s+[^,)]+'
    if not re.search(r'call\s+x86_regcallcc\s+\{[^\n]+?\}\s+@(?:"\\01)?__regcall3__combine\('+pieces+r'\)', llvm):
        raise SystemExit(prefix+'no se encontró la llamada física; evidencia: '+str(root))
    if args.indirecta and not re.search(r'call\s+x86_regcallcc\s+\{[^\n]+?\}\s+%[^\s(]+\('+pieces+r'\)', llvm):
        raise SystemExit(prefix+'no se encontró la llamada por puntero; evidencia: '+str(root))
    run(prefix+'ns-build', [args.nsc, '--emit-project-oop-exe', str(case/project),
                     '--cpp-clang-root='+args.clang_root, '--no-artifact-cache', '-O'+str(opt), '-o', str(case/'out.exe')])
    run(prefix+'ns-run', [str(case/'out.exe')])
    print(prefix+'control C y NativeSharp correctos', flush=True)
print('OK: ocho comparaciones estática y DLL', flush=True)
case = root/'identidad-distinta'
case.mkdir()
for name in ('api.h', 'Main.ns', 'Probe.nsproj'):
    shutil.copyfile(src/name, case/name)
shutil.copyfile(root/'estatica-O0'/'api.lib', case/'api.lib')
header = (case/'api.h').read_text(encoding='utf-8')
header = header.replace('API Triple __attribute__((regcall)) combine(Triple a, Triple b, int bias);',
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
