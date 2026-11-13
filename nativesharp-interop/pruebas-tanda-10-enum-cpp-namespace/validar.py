"""T10-002: tipos enum homologos en namespaces C++ distintos."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
args = parser.parse_args()
fixture = Path(__file__).resolve().parent
repo = fixture.parents[1]
root = Path(tempfile.mkdtemp(prefix='nsc-t10-enum-cpp-namespace-', dir=r'A:\temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)

def run(label, command, cwd):
    result = subprocess.run(command, cwd=cwd, env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    log = root / (label + '.log')
    log.write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode)
                   + '\n' + result.stdout, encoding='utf-8')
    if result.returncode:
        raise SystemExit(label + ' fallo; evidencia: ' + str(log) + '\n' + result.stdout)
    return result.stdout

if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version'], repo):
    raise SystemExit('Se requiere Clang 23.1.0')
for opt in range(4):
    case = root / ('O' + str(opt))
    case.mkdir()
    for name in ('api.h', 'api.cpp', 'Control.cpp', 'Main.ns', 'EnumCpp.nsproj'):
        shutil.copyfile(fixture / name, case / name)
    flags = ['-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-O' + str(opt)]
    run('producer-O' + str(opt), ['clang.exe', *flags, '-c', str(case / 'api.cpp'), '-o', str(case / 'api.obj')], repo)
    run('library-O' + str(opt), ['llvm-lib.exe', '/nologo', '/out:' + str(case / 'api.lib'), str(case / 'api.obj')], repo)
    run('control-O' + str(opt), ['clang.exe', *flags, '-c', str(case / 'Control.cpp'), '-o', str(case / 'control.obj')], repo)
    run('control-link-O' + str(opt), ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console',
        '/nodefaultlib', '/out:' + str(case / 'control.exe'), str(case / 'control.obj')], repo)
    run('control-run-O' + str(opt), [str(case / 'control.exe')], case)
    run('nsc-O' + str(opt), [args.nsc, '--emit-project-oop-exe', str(case / 'EnumCpp.nsproj'),
        '--library-reference-root', str(case), '--cpp-clang-root=' + args.clang_root,
        '--no-artifact-cache', '-O' + str(opt), '-o', str(case / 'native.exe')], repo)
    run('native-run-O' + str(opt), [str(case / 'native.exe')], case)
    (case / 'Main.ns').write_text('int Main() { return (int)First.Value == 10 && (int)Second.Value == 20 ? 0 : 1; }\n', encoding='utf-8')
    run('value-nsc-O' + str(opt), [args.nsc, '--emit-project-oop-exe', str(case / 'EnumCpp.nsproj'),
        '--library-reference-root', str(case), '--cpp-clang-root=' + args.clang_root,
        '--no-artifact-cache', '-O' + str(opt), '-o', str(case / 'value.exe')], repo)
    run('value-run-O' + str(opt), [str(case / 'value.exe')], case)
    (case / 'Main.ns').write_text('int Main() { First.Box x; x.field = First.Value; Second.Box y; y.field = Second.Value; return (int)x.field == 10 && (int)y.field == 20 ? 0 : 1; }\n', encoding='utf-8')
    run('field-nsc-O' + str(opt), [args.nsc, '--emit-project-oop-exe', str(case / 'EnumCpp.nsproj'),
        '--library-reference-root', str(case), '--cpp-clang-root=' + args.clang_root,
        '--no-artifact-cache', '-O' + str(opt), '-o', str(case / 'field.exe')], repo)
    run('field-run-O' + str(opt), [str(case / 'field.exe')], case)
    print('O' + str(opt) + ': Clang y NativeSharp OK', flush=True)
case = root / 'O0'
(case / 'Main.ns').write_text('int Main() { First.Mode value = Second.Value; return 0; }\n', encoding='utf-8')
command = [args.nsc, '--dump-project-oop-llvm', str(case / 'EnumCpp.nsproj'),
           '--library-reference-root', str(case), '--cpp-clang-root=' + args.clang_root]
result = subprocess.run(command, cwd=repo, env=env, stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT, text=True, errors='replace')
log = root / 'rechazo-identidad.log'
log.write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
if result.returncode == 0 or "cannot initialize local 'value'" not in result.stdout:
    raise SystemExit('No se rechazo la identidad distinta; evidencia: ' + str(log) + '\n' + result.stdout)
print('OK: dos enums homonimos en namespaces distintos', flush=True)
