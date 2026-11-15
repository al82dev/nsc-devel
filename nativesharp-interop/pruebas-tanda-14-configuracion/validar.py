"""T14-004, T14-005 y T14-011: condiciones del preprocesador, identidad de la sesion y dependencias.

El encabezado elige una funcion inline y una macro con `#if` sobre una macro que define el proyecto
(`<Define Value="..." />`) e incluye otro encabezado que incluye un tercero. El mismo proyecto se compila con
la cache de artefactos de NSC activa y se cambia, entre compilaciones, el valor de `Define`, luego un
`Undefine`, luego el tercer encabezado: cada ejecutable devuelve el valor que da la configuracion vigente,
igual que un control C++ compilado por Clang 23.1.0 con las mismas definiciones. Una segunda compilacion
sin cambios reutiliza la cache y da el mismo resultado. Con `--dll` la funcion no inline vive en una DLL.
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
root = Path(tempfile.mkdtemp(prefix='nsc-t14-configuracion-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)

HEADER = '''#pragma once
#include "middle.h"
#if defined(T14_BUILD_DLL)
#define T14_API __declspec(dllexport)
#elif defined(T14_USE_DLL)
#define T14_API __declspec(dllimport)
#else
#define T14_API
#endif
T14_API int t14_anchor() noexcept;
#if defined(T14_MODE) && T14_MODE == 2
#define T14_VALUE 20
inline int mode() noexcept { return 2 + T14_DEEP; }
#elif defined(T14_MODE)
#define T14_VALUE 10
inline int mode() noexcept { return 1 + T14_DEEP; }
#else
#define T14_VALUE 0
inline int mode() noexcept { return T14_DEEP; }
#endif
'''
MIDDLE = '#pragma once\n#include "deep.h"\n'


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


def control(label, defines, folder):
    (folder / 'Control.cpp').write_text('#include "api.h"\nint main() { return mode() + T14_VALUE + t14_anchor(); }\n',
                                        encoding='utf-8')
    run(label + '-control-obj', ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20',
                                 *['-D' + d for d in defines + DLL_DEFINES], '-c', 'Control.cpp', '-o', 'control.obj'], folder)
    run(label + '-control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console', '/nodefaultlib',
                                  '/out:control.exe', 'control.obj', 'api.lib'], folder)
    return run(label + '-control-run', [str(folder / 'control.exe')], folder, expected=None).returncode


DLL_DEFINES = ['T14_USE_DLL'] if args.dll else []
if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version'], root).stdout:
    raise SystemExit('Se requiere Clang 23.1.0')
cache = root / 'cache'
for opt in range(4):
    folder = root / f'O{opt}'
    folder.mkdir()
    (folder / 'api.h').write_text(HEADER, encoding='utf-8')
    (folder / 'middle.h').write_text(MIDDLE, encoding='utf-8')
    (folder / 'deep.h').write_text('#pragma once\n#define T14_DEEP 100\n', encoding='utf-8')
    (folder / 'api.cpp').write_text('#include "api.h"\nint t14_anchor() noexcept { return 0; }\n', encoding='utf-8')
    run(f'O{opt}-api', ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20',
                        *(['-DT14_BUILD_DLL'] if args.dll else []), '-c', 'api.cpp', '-o', 'api.obj'], folder)
    if args.dll:
        run(f'O{opt}-dll', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib', '/out:api.dll',
                            '/implib:api.lib', 'api.obj'], folder)
    else:
        run(f'O{opt}-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], folder)
    (folder / 'Main.ns').write_text('int Main() { return mode() + T14_VALUE + t14_anchor(); }\n', encoding='utf-8')
    runtime = ' Runtime="api.dll"' if args.dll else ''
    dll_define = '<Define Value="T14_USE_DLL" />' if args.dll else ''
    steps = [
        ('define-1', ['T14_MODE=1'], '<Define Value="T14_MODE=1" />', None),
        ('define-1-cache', ['T14_MODE=1'], '<Define Value="T14_MODE=1" />', None),
        ('define-2', ['T14_MODE=2'], '<Define Value="T14_MODE=2" />', None),
        ('undefine', [], '<Define Value="T14_MODE=2" /><Undefine Value="T14_MODE" />', None),
        ('deep', [], '<Undefine Value="T14_MODE" />', '#pragma once\n#define T14_DEEP 200\n'),
    ]
    for name, defines, children, deep in steps:
        if deep:
            (folder / 'deep.h').write_text(deep, encoding='utf-8')
        (folder / 'App.nsproj').write_text(
            '<Project><PropertyGroup><Name>T14App</Name></PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
            '<CppLibraryReference Header="api.h" Language="C++" Standard="c++20" Library="api.lib"' + runtime + '>' + dll_define + children +
            '</CppLibraryReference></ItemGroup></Project>', encoding='utf-8')
        tag = f'O{opt}-{name}'
        expected = control(tag, defines, folder)
        run(tag + '-build', [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
                             '--artifact-cache=' + str(cache), '-O' + str(opt), '-o', 'app.exe'], folder)
        result = run(tag + '-run', [str(folder / 'app.exe')], folder, expected=None).returncode
        if result != expected:
            raise SystemExit(f'{tag}: NativeSharp devolvio {result}, Clang {expected}; evidencia: {root}')
        print(f'{tag}: NativeSharp y Clang devuelven {result}', flush=True)
print('OK: condiciones del preprocesador, identidad de la sesion y dependencias, frente a Clang; evidencia:', root)
