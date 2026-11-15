"""T14-007, T14-009 y T14-010: punteros a miembro, headers precompilados y modulos C++.

- Una funcion C++ que recibe o devuelve un puntero a miembro no tiene firma NativeSharp: su uso se rechaza
  con el motivo y la funcion del mismo encabezado sin punteros a miembro se llama.
- Una biblioteca compilada con un header precompilado (`-include-pch`) se consume desde el encabezado fuente;
  la sesion de NSC analiza el encabezado con su propia configuracion y no lee el .pch.
- Un encabezado que importa un modulo C++20 (`import t14;`) se rechaza con el diagnostico de Clang, porque la
  sesion no recibe la interfaz compilada del modulo; la contraparte con el encabezado clasico pasa.
Cada caso positivo se compara con un control C++ compilado por Clang 23.1.0. Con `--dll` las funciones no
inline de los casos positivos viven en una DLL.
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
root = Path(tempfile.mkdtemp(prefix='nsc-t14-especiales-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)
CXX = ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20']


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


API = ('#if defined(T14_BUILD_DLL)\n#define T14_API __declspec(dllexport)\n#elif defined(T14_USE_DLL)\n'
       '#define T14_API __declspec(dllimport)\n#else\n#define T14_API\n#endif\n')
DLL_DEFINES = ['-DT14_USE_DLL'] if args.dll else []


def library(label, folder, compile_args):
    run(label + '-api', [*CXX, *(['-DT14_BUILD_DLL'] if args.dll else []), *compile_args, '-c', 'api.cpp', '-o', 'api.obj'],
        folder)
    if args.dll:
        run(label + '-dll', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib', '/out:api.dll',
                             '/implib:api.lib', 'api.obj'], folder)
    else:
        run(label + '-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], folder)


def project(folder, body, header='api.h'):
    (folder / 'Main.ns').write_text('int Main() { ' + body + ' }\n', encoding='utf-8')
    (folder / 'App.nsproj').write_text(
        '<Project><PropertyGroup><Name>T14App</Name></PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
        '<CppLibraryReference Header="' + header + '" Language="C++" Standard="c++20" Library="api.lib"' +
        (' Runtime="api.dll"><Define Value="T14_USE_DLL" /></CppLibraryReference>' if args.dll else ' />') +
        '</ItemGroup></Project>', encoding='utf-8')


def nsc(label, folder, opt, expected=0):
    return run(label, [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
                       '--no-artifact-cache', '-O' + str(opt), '-o', 'app.exe'], folder, expected)


def control(label, folder, main):
    (folder / 'Control.cpp').write_text(main, encoding='utf-8')
    run(label + '-control-obj', [*CXX, *DLL_DEFINES, '-c', 'Control.cpp', '-o', 'control.obj'], folder)
    run(label + '-control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console', '/nodefaultlib',
                                  '/out:control.exe', 'control.obj', 'api.lib'], folder)
    return run(label + '-control-run', [str(folder / 'control.exe')], folder, expected=None).returncode


if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version'], root).stdout:
    raise SystemExit('Se requiere Clang 23.1.0')

# Pointers to members. / Punteros a miembro.
folder = root / 'puntero-a-miembro'
folder.mkdir()
(folder / 'api.h').write_text('#pragma once\n' + API + '''namespace T14 {
struct S { int x; int y; };
inline int S::* pickY() noexcept { return &S::y; }
inline int get(const S* s, int S::* m) noexcept { return s->*m; }
T14_API int getY(const S* s) noexcept;
}
''', encoding='utf-8')
(folder / 'api.cpp').write_text('#include "api.h"\nnamespace T14 { int getY(const S* s) noexcept { return s->y; } }\n',
                                encoding='utf-8')
library('miembro', folder, [])
project(folder, 'T14.S s; s.x = 1; s.y = 7; return T14.get(&s, T14.pickY());')
output = nsc('negativo-miembro', folder, 0, expected=1).stdout
if 'pointer to member' not in output:
    print(output[-2000:], flush=True)
    raise SystemExit(f'negativo puntero-a-miembro: falta el diagnostico esperado; evidencia: {root}')
print('negativo puntero-a-miembro: rechazado con su motivo', flush=True)
expected = control('miembro', folder, '#include "api.h"\nint main() { T14::S s; s.x = 1; s.y = 7; return T14::getY(&s); }\n')
for opt in range(4):
    project(folder, 'T14.S s; s.x = 1; s.y = 7; return T14.getY(&s);')
    nsc(f'miembro-contraparte-O{opt}', folder, opt)
    result = run(f'miembro-contraparte-O{opt}-run', [str(folder / 'app.exe')], folder, expected=None).returncode
    if result != expected:
        raise SystemExit(f'miembro-contraparte-O{opt}: NativeSharp {result}, Clang {expected}; evidencia: {root}')
print(f'contraparte puntero-a-miembro: NativeSharp y Clang devuelven {expected}, O0-O3', flush=True)

# A library built with a precompiled header. / Una biblioteca compilada con un header precompilado.
folder = root / 'pch'
folder.mkdir()
(folder / 'api.h').write_text('#pragma once\n' + API + 'namespace T14 { T14_API int answer(int x) noexcept; '
                              'inline int base() noexcept { return 40; } }\n', encoding='utf-8')
(folder / 'api.cpp').write_text('namespace T14 { int answer(int x) noexcept { return base() + x; } }\n', encoding='utf-8')
run('pch-header', [*CXX, *(['-DT14_BUILD_DLL'] if args.dll else []), '-x', 'c++-header', 'api.h', '-o', 'api.pch'],
    folder)
library('pch', folder, ['-include-pch', 'api.pch'])
expected = control('pch', folder, '#include "api.h"\nint main() { return T14::answer(2); }\n')
for opt in range(4):
    project(folder, 'return T14.answer(2);')
    nsc(f'pch-O{opt}', folder, opt)
    result = run(f'pch-O{opt}-run', [str(folder / 'app.exe')], folder, expected=None).returncode
    if result != expected:
        raise SystemExit(f'pch-O{opt}: NativeSharp {result}, Clang {expected}; evidencia: {root}')
print(f'pch: biblioteca con header precompilado; NativeSharp y Clang devuelven {expected}, O0-O3', flush=True)

# A C++20 module. / Un modulo C++20.
folder = root / 'modulo'
folder.mkdir()
(folder / 't14.cppm').write_text('export module t14;\nexport namespace T14 { inline int seven() noexcept { return 7; } }\n',
                                 encoding='utf-8')
run('modulo-pcm', [*CXX, '--precompile', 't14.cppm', '-o', 't14.pcm'], folder)
(folder / 'api.h').write_text('#pragma once\nimport t14;\n', encoding='utf-8')
(folder / 'classic.h').write_text('#pragma once\nnamespace T14 { inline int seven() noexcept { return 7; } }\n',
                                  encoding='utf-8')
(folder / 'api.cpp').write_text('int t14_anchor() { return 0; }\n', encoding='utf-8')
run('modulo-api', [*CXX, '-c', 'api.cpp', '-o', 'api.obj'], folder)
run('modulo-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], folder)
dll_mode = args.dll
args.dll = False  # the module case has no exported function / el caso de modulo no exporta funciones
project(folder, 'return T14.seven() - 7;')
output = nsc('negativo-modulo', folder, 0, expected=1).stdout
if "module 't14' not found" not in output:
    print(output[-2000:], flush=True)
    raise SystemExit(f'negativo modulo: falta el diagnostico de Clang; evidencia: {root}')
print('negativo modulo: rechazado con el diagnostico de Clang', flush=True)
project(folder, 'return T14.seven() - 7;', 'classic.h')
nsc('modulo-contraparte', folder, 0)
if run('modulo-contraparte-run', [str(folder / 'app.exe')], folder, expected=None).returncode != 0:
    raise SystemExit(f'modulo-contraparte: el encabezado clasico no devuelve 0; evidencia: {root}')
print('contraparte modulo: el encabezado clasico pasa', flush=True)
args.dll = dll_mode
print('OK: punteros a miembro, headers precompilados y modulos C++, frente a Clang; evidencia:', root)
