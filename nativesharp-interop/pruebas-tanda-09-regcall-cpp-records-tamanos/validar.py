"""Compara regcall de un record C++ trivial en Clang y NativeSharp."""
import argparse
import os
import re
from pathlib import Path
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
args = parser.parse_args()
fixture = Path(__file__).resolve().parent
root = Path(tempfile.mkdtemp(prefix='nsc-t09-cpp-varied-', dir=r'A:\temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)

def run(label, command):
    result = subprocess.run(command, cwd=fixture, env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    log = root/(label+'.log')
    log.write_text('COMMAND: '+repr(command)+'\nEXIT: '+str(result.returncode)+'\n'+result.stdout, encoding='utf-8')
    if result.returncode:
        raise SystemExit(label+': fallo; evidencia: '+str(log))
    return result.stdout

if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version']):
    raise SystemExit('Se requiere Clang 23.1.0')
for mode in ('estatica', 'dll'):
    for opt in range(4):
        name = mode+'-O'+str(opt)
        case = root/name
        case.mkdir()
        for file in ('api.h', 'api-dll.h', 'api.cpp', 'Control.cpp', 'Main.ns', 'Probe.nsproj', 'ProbeDll.nsproj'):
            shutil.copyfile(fixture/file, case/file)
        flags = ['-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-O'+str(opt)]
        run(name+'-producer', ['clang.exe', *flags, *(['-DAPI_DLL_BUILD'] if mode == 'dll' else []),
                               '-c', str(case/'api.cpp'), '-o', str(case/'api.obj')])
        symbols = run(name+'-symbols', ['llvm-nm.exe', '--defined-only', str(case/'api.obj')])
        found = {}
        for function in ('nested', 'array', 'fetchNested', 'fetchArray'):
            matches = re.findall(r'\?'+function+r'@T09HfaCpp@@\S+', symbols)
            if len(matches) != 1: raise SystemExit(name+': simbolo C++ inesperado; evidencia: '+str(root))
            found[function] = matches[0]
        if mode == 'estatica':
            library, project, control_flags = 'api.lib', 'Probe.nsproj', []
            run(name+'-archive', ['llvm-lib.exe', '/nologo', '/out:'+str(case/library), str(case/'api.obj')])
        else:
            library, project, control_flags = 'api-dll.lib', 'ProbeDll.nsproj', ['-DAPI_DLL_IMPORT']
            run(name+'-dll', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib', *['/export:'+symbol for symbol in found.values()],
                             '/out:'+str(case/'api.dll'), '/implib:'+str(case/library), str(case/'api.obj')])
        run(name+'-control-object', ['clang.exe', *flags, *control_flags, '-c', str(case/'Control.cpp'), '-o', str(case/'control.obj')])
        run(name+'-control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console', '/nodefaultlib',
                                 '/out:'+str(case/'control.exe'), str(case/'control.obj'), str(case/library)])
        run(name+'-control-run', [str(case/'control.exe')])
        ir = run(name+'-llvm', [args.nsc, '--dump-project-oop-llvm', str(case/project),
                 '--library-reference-root', str(case), '--cpp-clang-root='+args.clang_root])
        for count, function in ((5, 'nested'), (5, 'array')):
            result_type = r'\{\s*\{\s*float,\s*float\s*\},\s*float\s*\}' if function == 'nested' else r'\{\s*\[3 x float\]\s*\}'
            arguments = r',\s*'.join([r'float\s+[^,)]+']*count) + r',\s*i32\s+[^,)]+'
            direct = r'(?:call|invoke)\s+x86_regcallcc\s+'+result_type+r'\s+@[^\n]*'+re.escape(found[function])+r'[^\n]*\('+arguments+r'\)'
            indirect = r'call\s+x86_regcallcc\s+'+result_type+r'\s+%[^\s(]+\('+arguments+r'\)'
            if not re.search(direct, ir) or not re.search(indirect, ir):
                raise SystemExit(name+': falta llamada C++ '+function+' directa o indirecta; evidencia: '+str(root))
        out = case/'out'/'Probe.exe'
        out.parent.mkdir()
        run(name+'-nsc', [args.nsc, '--emit-project-oop-exe', str(case/project),
                         '--library-reference-root', str(case), '--cpp-clang-root='+args.clang_root,
                         '-O'+str(opt), '--no-artifact-cache', '-o', str(out)])
        run(name+'-nsc-run', [str(out)])
        print(name+': C++ y NativeSharp correctos', flush=True)
print('OK: ocho comparaciones; evidencia:', root, flush=True)
