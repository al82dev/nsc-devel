"""Contraste puntual de record indirecto SysV en estatica y DLL, O0-O3."""
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
root = Path(tempfile.mkdtemp(prefix='nsc-t09-sysv-indirecta-record-gpr-matriz-', dir=r'A:\temp'))
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
pattern = r'call\s+x86_64_sysvcc\s+i32\s+\(i32,\s*\.\.\.\)\s+%[^\(]+\(i32\s+[^,]+,' + r'(?:\s*i32\s+[^,]+,){5}\s*ptr\s+(?:noundef\s+)?byval\([^)]+\)\s+align\s+8\s+[^,]+,\s*ptr\s+'
for mode in ('estatica', 'dll'):
    for opt in range(4):
        prefix = mode + '-O' + str(opt)
        case = root / prefix
        case.mkdir()
        for name in ('api.h', 'api.c', 'producer.s', 'Control.c', 'Main.ns', 'Static.nsproj'):
            shutil.copyfile(fixture / name, case / name)
        flags = ['-target', 'x86_64-pc-windows-msvc', '-std=c11', '-O' + str(opt)]
        run(prefix + '-producer', ['clang.exe', *flags, '-c', str(case / 'api.c'), '-o', str(case / 'api.obj')])
        run(prefix + '-asm', ['clang.exe', *flags, '-c', str(case / 'producer.s'), '-o', str(case / 'producer.obj')])
        if mode == 'estatica':
            run(prefix + '-library', ['llvm-lib.exe', '/nologo', '/out:' + str(case / 'api.lib'), str(case / 'api.obj'), str(case / 'producer.obj')])
        else:
            run(prefix + '-dll', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib',
                '/export:t09_get_var_record_callback', '/out:' + str(case / 'api.dll'),
                '/implib:' + str(case / 'api-dll.lib'), str(case / 'api.obj'), str(case / 'producer.obj')])
            (case / 'api-dll.h').write_text((case / 'api.h').read_text(encoding='utf-8').replace(
                'T09IndirectVarCallback t09_get_var_record_callback',
                '__declspec(dllimport) T09IndirectVarCallback t09_get_var_record_callback'), encoding='utf-8')
            (case / 'Dll.nsproj').write_text((case / 'Static.nsproj').read_text(encoding='utf-8').replace(
                'Header="api.h"', 'Header="api-dll.h"').replace(
                'Library="api.lib"', 'Library="api-dll.lib" Runtime="api.dll"'), encoding='utf-8')
        run(prefix + '-control-obj', ['clang.exe', *flags, '-c', str(case / 'Control.c'), '-o', str(case / 'control.obj')])
        run(prefix + '-control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console',
            '/nodefaultlib', '/out:' + str(case / 'control.exe'), str(case / 'control.obj'),
            str(case / ('api.lib' if mode == 'estatica' else 'api-dll.lib'))])
        run(prefix + '-control-run', [str(case / 'control.exe')])
        common = [str(case / ('Static.nsproj' if mode == 'estatica' else 'Dll.nsproj')),
                  '--cpp-clang-root=' + args.clang_root, '--library-reference-root', str(case)]
        ir = run(prefix + '-ns-ir', [args.nsc, '--dump-project-oop-llvm', *common])
        if not re.search(pattern, ir):
            raise SystemExit(prefix + ': faltan piezas SysV; evidencia: ' + str(root))
        run(prefix + '-ns-build', [args.nsc, '--emit-project-oop-exe', *common, '--no-artifact-cache',
            '-O' + str(opt), '-o', str(case / 'ns.exe')])
        run(prefix + '-ns-run', [str(case / 'ns.exe')])
        print(prefix + ': C y NativeSharp pasaron', flush=True)
print('OK: ocho comparaciones C/NativeSharp; evidencia:', root, flush=True)
