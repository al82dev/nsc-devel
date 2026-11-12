"""Compara el paso y retorno regcall de un record flotante entre C y NS."""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--nsc')
parser.add_argument('--clang-root')
parser.add_argument('--solo-c', action='store_true')
parser.add_argument('--solo-directo', action='store_true')
args = parser.parse_args()
if not args.solo_c and (not args.nsc or not args.clang_root):
    parser.error('--nsc y --clang-root son obligatorios')
fixture = Path(__file__).resolve().parent
root = Path(tempfile.mkdtemp(prefix='nsc-t09-regcall-mixed-', dir=r'A:\temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)

def run(label, command):
    result = subprocess.run(command, cwd=fixture, env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    log = root / (label + '.log')
    log.write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) +
                   '\n' + result.stdout, encoding='utf-8')
    if result.returncode:
        raise SystemExit(label + ': fallo; evidencia: ' + str(log))
    return result.stdout

if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version']):
    raise SystemExit('Se requiere Clang 23.1.0')
for mode in ('estatica', 'dll'):
    for opt in range(4):
        name = mode + '-O' + str(opt)
        case = root / name
        case.mkdir()
        for file in ('api.h', 'api-dll.h', 'api.c', 'Control.c', 'Main.ns',
                     'VectorCall.nsproj', 'VectorCallDll.nsproj'):
            shutil.copyfile(fixture / file, case / file)
        if args.solo_directo:
            shutil.copyfile(fixture/'MainDirect.ns', case/'Main.ns')
        flags = ['-target', 'x86_64-pc-windows-msvc', '-std=c11', '-O' + str(opt)]
        producer_flags = ['-DAPI_DLL_BUILD'] if mode == 'dll' else []
        run(name+'-producer', ['clang.exe', *flags, *producer_flags, '-c', str(case/'api.c'), '-o', str(case/'api.obj')])
        if mode == 'estatica':
            library, project, control_flags = 'api.lib', 'VectorCall.nsproj', []
            run(name+'-archive', ['llvm-lib.exe', '/nologo', '/out:'+str(case/library), str(case/'api.obj')])
        else:
            library, project, control_flags = 'api-dll.lib', 'VectorCallDll.nsproj', ['-DAPI_DLL_IMPORT']
            run(name+'-dll', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib',
                             '/export:__regcall3__t09_nested', '/export:__regcall3__t09_array',
                             '/export:t09_get_nested', '/export:t09_get_array',
                             '/out:'+str(case/'api.dll'), '/implib:'+str(case/library), str(case/'api.obj')])
        run(name+'-control-object', ['clang.exe', *flags, *control_flags, '-c', str(case/'Control.c'), '-o', str(case/'control.obj')])
        run(name+'-control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console', '/nodefaultlib',
                                 '/out:'+str(case/'control.exe'), str(case/'control.obj'), str(case/library)])
        run(name+'-control-run', [str(case/'control.exe')])
        if not args.solo_c:
            llvm_ir = run(name+'-llvm', [args.nsc, '--dump-project-oop-llvm', str(case/project),
                          '--library-reference-root', str(case), '--cpp-clang-root='+args.clang_root])
            for count, function in ((3, 'nested'), (3, 'array')):
                result_type = r'\{[^\n]+?\}'
                arguments = r',\s*'.join([r'float\s+[^,)]+']*count) + r',\s*i32\s+[^,)]+'
                targets = [r'@(?:"\\01)?__regcall3__t09_'+function]
                if not args.solo_directo:
                    targets.append(r'%[^\s(]+')
                for target in targets:
                    if not re.search(r'call\s+x86_regcallcc\s+'+result_type+r'\s+'+target+r'\('+arguments+r'\)', llvm_ir):
                        raise SystemExit(name+': falta llamada regcall '+function+' directa o indirecta; evidencia: '+str(root))
            out = case/'out'/'VectorCall.exe'
            out.parent.mkdir()
            run(name+'-nsc', [args.nsc, '--emit-project-oop-exe', str(case/project),
                             '--library-reference-root', str(case), '--cpp-clang-root='+args.clang_root,
                             '-O'+str(opt), '--no-artifact-cache', '-o', str(out)])
            run(name+'-nsc-run', [str(out)])
            if opt == 0:
                cache = case/'artifact-cache'
                cache_command = [args.nsc, '--emit-project-oop-exe', str(case/project),
                    '--library-reference-root', str(case), '--cpp-clang-root='+args.clang_root,
                    '--artifact-cache='+str(cache), '--time', '-O0', '-o', str(out)]
                for expected in ('not-found', 'hit'):
                    out.unlink(missing_ok=True)
                    log = run(name+'-cache-'+expected, cache_command)
                    if 'compilation-bundle-cache: '+expected not in log:
                        raise SystemExit(name+': estado de cache inesperado; evidencia: '+str(root))
                    run(name+'-cache-run-'+expected, [str(out)])
        print(name + (': control C y NativeSharp correctos' if not args.solo_c else ': control C correcto'), flush=True)
print('OK:', root, flush=True)
