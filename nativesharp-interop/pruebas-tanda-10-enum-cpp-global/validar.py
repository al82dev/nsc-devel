"""T10-002: enum C++ global, conversion explicita y copia; contraste con Clang."""
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
root = Path(tempfile.mkdtemp(prefix='nsc-t10-enum-cpp-global-', dir=r'A:\temp'))
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
    print('O' + str(opt) + ': Clang y NativeSharp OK', flush=True)
case = root / 'regresion-c'
shutil.copytree(root / 'O0', case)
(case / 'api.h').write_text('typedef enum Mode { Chosen = 10, Changed = 11 } Mode;\nint enum_anchor(void);\n', encoding='utf-8')
(case / 'EnumCpp.nsproj').write_text((case / 'EnumCpp.nsproj').read_text(encoding='utf-8').replace('Language="C++" Standard="c++20"', 'Language="C" Standard="c11"'), encoding='utf-8')
run('c-nsc', [args.nsc, '--emit-project-oop-exe', str(case / 'EnumCpp.nsproj'),
    '--library-reference-root', str(case), '--cpp-clang-root=' + args.clang_root,
    '--no-artifact-cache', '-O0', '-o', str(case / 'c-native.exe')], repo)
run('c-run', [str(case / 'c-native.exe')], case)
print('Regresion C: OK', flush=True)
for name, declaration in (
    ('scoped', 'enum class Mode : int { Chosen = 10 };'),
    ('namespace', 'namespace Other { enum Mode : int { Chosen = 10 }; }'),
    ('unsigned', 'enum Mode : unsigned int { Chosen = 10 };'),
):
    case = root / ('rechazo-' + name)
    shutil.copytree(root / 'O0', case)
    (case / 'api.h').write_text(declaration + '\n', encoding='utf-8')
    (case / 'Main.ns').write_text('int Main() { Mode value; return 0; }\n', encoding='utf-8')
    command = [args.nsc, '--dump-project-oop-llvm', str(case / 'EnumCpp.nsproj'),
        '--library-reference-root', str(case), '--cpp-clang-root=' + args.clang_root]
    result = subprocess.run(command, cwd=repo, env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    log = root / ('rechazo-' + name + '.log')
    log.write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode)
                   + '\n' + result.stdout, encoding='utf-8')
    if result.returncode == 0 or "unknown type name 'Mode'" not in result.stdout:
        raise SystemExit('Rechazo inesperado: ' + str(log) + '\n' + result.stdout)
    print('Rechazo ' + name + ': OK', flush=True)
print('OK: enum C++ global por conversion explicita y copia', flush=True)
