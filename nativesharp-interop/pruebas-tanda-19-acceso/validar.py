"""T19-012: miembros de instancia C++ protegidos y privados.

Una clase C++ con campos y metodos de instancia privados y protegidos se importa con su acceso. NativeSharp usa
los publicos, cuyo resultado depende de los privados, como un control C++ de Clang 23.1.0, O0-O3, en estatica y
DLL. Cada uso de un campo o metodo privado o protegido se rechaza con `member 'x' is not accessible`, igual que
Clang lo rechaza.
"""
import argparse
import os
import subprocess
import tempfile
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
parser.add_argument('--dll', action='store_true')
args = parser.parse_args()
root = Path(tempfile.mkdtemp(prefix='nsc-t19-acceso-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)
CXX = ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20']
API = '''#pragma once
#if defined(T19_EXPORT)
#define API __declspec(dllexport)
#elif defined(T19_IMPORT)
#define API __declspec(dllimport)
#else
#define API
#endif
'''
HEADER = '''namespace A {
class API K {
    int secret = 1;
    int hidden() const noexcept { return 5; }
protected:
    int guarded = 2;
    int shielded() const noexcept;
public:
    int open = 3;
    K() noexcept {}
    int sum() const noexcept;
};
}
'''
SOURCE = '''#include "api.h"
extern "C" __declspec(selectany) int _fltused = 0;
namespace A {
int K::shielded() const noexcept { return guarded * 10; }
int K::sum() const noexcept { return secret + guarded + open + hidden() + shielded(); }
}
'''
CPP_BODY = 'A::K k; k.open = 10; return k.sum() + k.open;'
NS_BODY = 'A.K k = new A.K(); k.open = 10; return k.sum() + k.open;'


def run(label, command, cwd, expected=0):
    result = subprocess.run(command, cwd=cwd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, errors='replace')
    log = root / (label + '.log')
    log.write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' + result.stdout,
                   encoding='utf-8')
    if expected is not None and ((expected == 0 and result.returncode) or (expected != 0 and result.returncode == 0)):
        print(result.stdout[-3000:], flush=True)
        raise SystemExit(label + ': codigo ' + str(result.returncode) + ' inesperado; evidencia: ' + str(log))
    return result


def project(case, body):
    runtime = ' Runtime="api.dll"' if args.dll else ''
    (case / 'Main.ns').write_text('int Main()\n{\n    ' + body + '\n}\n', encoding='utf-8')
    (case / 'App.nsproj').write_text(
        '<Project><PropertyGroup><Name>T19Acceso</Name></PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
        '<CppLibraryReference Header="api.h" Language="C++" Standard="c++20" Library="api.lib"' + runtime + ' />'
        '</ItemGroup></Project>', encoding='utf-8')


def nsc(case, opt, expected=0):
    return run(f'{case.name}-nsc-O{opt}', [args.nsc, '--emit-project-oop-exe', 'App.nsproj',
                                           '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache',
                                           '-O' + str(opt), '-o', 'app.exe'], case, expected=expected).stdout


if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version'], root).stdout:
    raise SystemExit('Se requiere Clang 23.1.0')
library = root / 'lib'
library.mkdir()
(library / 'api.h').write_text(API + HEADER, encoding='utf-8')
(library / 'api.cpp').write_text(SOURCE, encoding='utf-8')
run('api', [*CXX, '-O2', *(['-DT19_EXPORT'] if args.dll else []), '-c', 'api.cpp', '-o', 'api.obj'], library)
if args.dll:
    run('api-dll', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib', '/out:api.dll', '/implib:api.lib',
                    'api.obj'], library)
    header = '#define T19_IMPORT 1\n' + API + HEADER
else:
    run('api-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], library)
    header = API + HEADER


def prepare(name):
    case = root / name
    case.mkdir()
    (case / 'api.h').write_text(header, encoding='utf-8')
    for item in ('api.lib', *(('api.dll',) if args.dll else ())):
        (case / item).write_bytes((library / item).read_bytes())
    return case


for opt in range(4):
    case = prepare(f'O{opt}')
    (case / 'control.cpp').write_text('#include "api.h"\nextern "C" __declspec(selectany) int _fltused = 0;\n'
                                      'int main() { ' + CPP_BODY + ' }\n', encoding='utf-8')
    run(f'O{opt}-control', [*CXX, '-O' + str(opt), 'control.cpp', 'api.lib', '-fuse-ld=lld', '-nostdlib',
                            '-Wl,/entry:main', '-o', 'control.exe'], case)
    control = run(f'O{opt}-control-run', [str(case / 'control.exe')], case, expected=None).returncode
    project(case, NS_BODY)
    nsc(case, opt)
    result = run(f'O{opt}-run', [str(case / 'app.exe')], case, expected=None).returncode
    if result != control:
        raise SystemExit(f'O{opt}: NativeSharp devolvio {result}, Clang {control}; evidencia: {root}')
    print(f'O{opt}: NativeSharp y Clang devuelven {result}', flush=True)
# Each use of a protected or private member is rejected, by Clang too.
# Cada uso de un miembro protegido o privado se rechaza, tambien en Clang.
for label, cpp, ns, member, clang_text in (
        ('campo-privado', 'return k.secret;', 'return k.secret;', 'secret', "'secret' is a private member"),
        ('campo-protegido', 'return k.guarded;', 'return k.guarded;', 'guarded', "'guarded' is a protected member"),
        ('metodo-privado', 'return k.hidden();', 'return k.hidden();', 'hidden', "'hidden' is a private member"),
        ('metodo-protegido', 'return k.shielded();', 'return k.shielded();', 'shielded',
         "'shielded' is a protected member")):
    case = prepare(label)
    (case / 'control.cpp').write_text('#include "api.h"\nint main() { A::K k; ' + cpp + ' }\n', encoding='utf-8')
    clang = run(case.name + '-control', [*CXX, '-c', 'control.cpp', '-o', 'control.obj'], case, expected=1).stdout
    if clang_text not in clang:
        print(clang[-2000:], flush=True)
        raise SystemExit(f'{label}: Clang no rechaza el acceso; evidencia: {root}')
    project(case, 'A.K k = new A.K(); ' + ns)
    output = nsc(case, 0, expected=1)
    if "member '" + member + "' is not accessible" not in output:
        print(output[-2000:], flush=True)
        raise SystemExit(f'{label}: falta el diagnostico de acceso; evidencia: {root}')
    print(f'{label}: se rechaza el acceso, igual que Clang', flush=True)
print('OK: acceso a miembros C++ protegidos y privados, frente a Clang; evidencia:', root)
