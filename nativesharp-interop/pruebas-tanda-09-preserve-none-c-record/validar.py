"""Contrasta record C por valor en preserve_none frente a Clang."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
args = parser.parse_args()
root = Path(tempfile.mkdtemp(prefix='nsc-t09-preserve-none-c-record-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)


def run(label, command):
    result = subprocess.run(command, cwd=root, env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    (root / (label + '.log')).write_text('COMMAND: ' + repr(command) + '\nEXIT: '
        + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
    if result.returncode:
        print(result.stdout, flush=True)
        raise SystemExit(label + ': fallo; evidencia: ' + str(root))
    return result.stdout


if 'clang version 23.1.0' not in run('version', ['clang.exe', '--version']):
    raise SystemExit('Se requiere Clang 23.1.0')
(root / 'api.h').write_text('typedef struct T09Small { int x, y; int padding[7]; } T09Small;\n'
    'T09Small __attribute__((preserve_none)) t09_c_record(T09Small p);\n', encoding='utf-8')
(root / 'api.c').write_text('#include "api.h"\n'
    'T09Small __attribute__((preserve_none)) t09_c_record(T09Small p) { p.x += 3; p.y -= 2; p.padding[6] += 4; return p; }\n', encoding='utf-8')
(root / 'Control.c').write_text('#include "api.h"\n'
    'void *memcpy(void *d, const void *s, unsigned long long n) { volatile unsigned char *o = d; '
    'const volatile unsigned char *i = s; for (unsigned long long k = 0; k < n; ++k) o[k] = i[k]; return d; }\n'
    'int main(void) { T09Small p; p.x = 10; p.y = 20; p.padding[6] = 30; '
    'T09Small q = t09_c_record(p); return p.x == 10 && p.y == 20 && p.padding[6] == 30 && '
    'q.x == 13 && q.y == 18 && q.padding[6] == 34 ? 0 : 1; }\n', encoding='utf-8')
(root / 'Main.ns').write_text('int Main() { T09Small p; p.x = 10; p.y = 20; p.padding[6] = 30; '
    'T09Small q = t09_c_record(p); return p.x == 10 && p.y == 20 && p.padding[6] == 30 && '
    'q.x == 13 && q.y == 18 && q.padding[6] == 34 ? 0 : 1; }\n', encoding='utf-8')
(root / 'Fixture.nsproj').write_text('<Project><PropertyGroup><Name>Fixture</Name></PropertyGroup><ItemGroup>'
    '<Compile Include="Main.ns" /><CppLibraryReference Header="api.h" Language="C" Standard="c17" '
    'Library="api.lib" /></ItemGroup></Project>', encoding='utf-8')
flags = ['-target', 'x86_64-pc-windows-msvc', '-std=c17', '-O0']
run('clang-api', ['clang.exe', *flags, '-c', 'api.c', '-o', 'api.obj'])
run('clang-ir', ['clang.exe', *flags, '-S', '-emit-llvm', 'api.c', '-o', '-'])
run('lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'])
run('clang-control', ['clang.exe', *flags, '-c', 'Control.c', '-o', 'control.obj'])
run('clang-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console', '/nodefaultlib',
    '/out:control.exe', 'control.obj', 'api.lib'])
run('clang-run', [str(root / 'control.exe')])
run('nsc-build', [args.nsc, '--emit-project-oop-exe', 'Fixture.nsproj', '--cpp-clang-root=' + args.clang_root,
    '--no-artifact-cache', '-O0', '-o', 'native.exe'])
run('nsc-run', [str(root / 'native.exe')])
print('OK: record C preserve_none O0; evidencia:', root)
