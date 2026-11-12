"""Contrasta llamadas C regcall desde fuente con DLL, O0–O3."""
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
parser.add_argument('--triple-double', action='store_true', help='comprueba los registros de tres double expandidos')
args = parser.parse_args()
base = Path(__file__).resolve().parent.parent
root = Path(tempfile.mkdtemp(prefix='nsc-t09-triple-double-c-dll-' if args.triple_double else 'nsc-t09-mixto-c-dll-', dir=r'A:\temp'))
env = dict(os.environ, TEMP=str(root), TMP=str(root))
print('Evidencia:', root, flush=True)

def run(case, name, command):
    result = subprocess.run(command, cwd=case, env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    log = case / (name + '.log')
    log.write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
    if result.returncode:
        raise SystemExit(name + ': fallo; evidencia: ' + str(log))
    return result.stdout

if 'clang version 23.1.0' not in run(root, 'clang-version', ['clang.exe', '--version']):
    raise SystemExit('Se requiere Clang 23.1.0')
cases = (('real', 'double', 'sum_pair', 'get_sum', 'i32'),
         ('int', 'i32', 'sum_pair', 'get_sum', 'i32'),
         ('float', 'float', 'sum_pair', 'get_sum', 'i32'),
         ('i64', 'i64', 'sum_pair', 'get_sum', 'i32'),
         ('short', 'i16', 'sum_pair', 'get_sum', 'i32'),
         ('bool', 'zeroext i1', 'sum_pair', 'get_sum', 'i32'),
         ('void', 'void', 'write_pair', 'get_write', 'ptr'),
         ('pointer', 'ptr', 'sum_pair', 'get_sum', 'ptr'))
for name, result_type, function, getter, tail in cases:
    if args.triple_double and name == 'real':
        continue
    prefix = 'pruebas-tanda-09-regcall-triple-double-' if args.triple_double else 'pruebas-tanda-09-regcall-record-mixto-'
    source = base / (prefix + name)
    for opt in range(4):
        case = root / (name + '-O' + str(opt))
        case.mkdir()
        for file in ('api.h', 'api-dll.h', 'api.c', 'Control.c', 'Main.ns'):
            shutil.copyfile(source / file, case / file)
        (case / 'Probe.nsproj').write_text('<Project><PropertyGroup><Name>Probe</Name></PropertyGroup><ItemGroup>'
            '<Compile Include="Main.ns" /><CppLibraryReference Header="api-dll.h" Language="C" Standard="c11" '
            'Library="api-dll.lib" Runtime="api.dll" /></ItemGroup></Project>', encoding='utf-8')
        flags = ['-target', 'x86_64-pc-windows-msvc', '-std=c11', '-O' + str(opt)]
        run(case, 'producer', ['clang.exe', *flags, '-DAPI_DLL_BUILD', '-c', 'api.c', '-o', 'api.obj'])
        run(case, 'dll', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib',
            '/export:__regcall3__' + function, '/export:' + getter,
            '/out:api.dll', '/implib:api-dll.lib', 'api.obj'])
        run(case, 'control-object', ['clang.exe', *flags, '-DAPI_DLL_IMPORT', '-c', 'Control.c', '-o', 'control.obj'])
        run(case, 'control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console', '/nodefaultlib',
            '/out:control.exe', 'control.obj', 'api-dll.lib'])
        run(case, 'control-run', [str(case / 'control.exe')])
        llvm = run(case, 'llvm', [args.nsc, '--dump-project-oop-llvm', str(case / 'Probe.nsproj'),
            '--library-reference-root', str(case), '--cpp-clang-root=' + args.clang_root])
        carrier = r'\s+'.join(result_type.split())
        record = r',\s*'.join([r'double\s+[^,)]+'] * 3) if args.triple_double else r'ptr\s+[^,)]+'
        arguments = record + r',\s*' + tail + r'\s+[^,)]+'
        for target in (r'@(?:"\\01)?__regcall3__' + function, r'%[^\s(]+'):
            if not re.search(r'call\s+x86_regcallcc\s+' + carrier + r'\s+' + target + r'\(' + arguments + r'\)', llvm):
                raise SystemExit(name + ': llamada fisica ausente; evidencia: ' + str(case / 'llvm.log'))
        output = case / 'out' / 'Probe.exe'
        output.parent.mkdir()
        run(case, 'nsc', [args.nsc, '--emit-project-oop-exe', str(case / 'Probe.nsproj'),
            '--library-reference-root', str(case), '--cpp-clang-root=' + args.clang_root,
            '--no-artifact-cache', '-O' + str(opt), '-o', str(output)])
        run(case, 'nsc-run', [str(output)])
        print(name + '-O' + str(opt) + ': correcto', flush=True)
print('OK:', (28 if args.triple_double else 32), 'comparaciones C con DLL; evidencia:', root, flush=True)
