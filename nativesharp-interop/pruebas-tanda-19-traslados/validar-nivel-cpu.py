"""T19-036: el nivel de CPU de una compilacion (`CpuLevel` del proyecto y `--cpu-level`).

- `--cpu-level=x86-64-v3` en la linea de ordenes da el nivel a un proyecto que no lo declara, y tiene prioridad
  sobre el `CpuLevel` del proyecto.
- Un valor desconocido en el proyecto o en la linea de ordenes se rechaza con su diagnostico.
- Una funcion C++ con un vector de 256 bits funciona con el nivel y se rechaza sin el.
- El IR de NSC lleva el CPU del nivel elegido en la maquina destino (`--print-codegen` no aplica a proyectos), y el
  ejecutable usa instrucciones AVX (`vmovups ymm`) solo con el nivel.

Uso: validar-nivel-cpu.py --nsc <nsc.exe> --clang-root <raiz de Clang>
"""
import argparse
import os
import subprocess
import tempfile
from pathlib import Path
# EN: The standard Core is consumed as the library built by the external SDK (compiler first, then libraries),
#     one package per optimization level, under NSC_STDLIB_PACKAGE_ROOT (default: build-clang/packages, where
#     NSC Builder installs them).
# ES: El Core estandar se consume como la biblioteca construida por el SDK externo (primero el compilador, despues
#     las bibliotecas), un paquete por nivel de optimizacion, bajo NSC_STDLIB_PACKAGE_ROOT (por defecto:
#     build-clang/packages, donde los instala NSC Builder).
STDLIB_PACKAGE_ROOT = Path(os.environ.get('NSC_STDLIB_PACKAGE_ROOT',
                                          Path(__file__).resolve().parents[2] / 'build-clang' / 'packages'))


def core_reference(opt):
    found = sorted(STDLIB_PACKAGE_ROOT.glob(f'NativeSharp.Core/*/windows-x64/O{opt}/NativeSharpCore.lib'))
    if not found:
        raise SystemExit(f'NativeSharp.Core O{opt} package not found under {STDLIB_PACKAGE_ROOT}; build the external '
                         'libraries first or set NSC_STDLIB_PACKAGE_ROOT')
    return '<LibraryReference Include="' + found[-1].as_posix() + '" />'

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
args = parser.parse_args()
root = Path(tempfile.mkdtemp(prefix='nsc-t19-nivel-cpu-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)
CORE = core_reference(0)
HEADER = '''namespace T {
typedef float f8 __attribute__((vector_size(32)));
float fsum8(f8 v) noexcept;
}
'''
SOURCE = '''#include "api.h"
extern "C" int _fltused = 0;
namespace T { float fsum8(f8 v) noexcept { float s = 0.0f; for (int i = 0; i < 8; ++i) s += v[i] * (float)(i + 1);
return s; } }
'''
MAIN = '''using NativeSharp.Core.Numerics;
int Main()
{
    Float32x8 a; a.X = 1.0f; a.Y = 2.0f; a.Z = 3.0f; a.W = 4.0f; a.A = 5.0f; a.B = 6.0f; a.C = 7.0f; a.D = 8.0f;
    return (int)T.fsum8(a);
}
'''


def run(label, command, cwd, expected=0):
    result = subprocess.run(command, cwd=cwd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                            errors='replace')
    log = root / (label + '.log')
    log.write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' + result.stdout,
                   encoding='utf-8')
    if expected is not None and ((expected == 0 and result.returncode) or (expected != 0 and result.returncode == 0)):
        print(result.stdout[-3000:], flush=True)
        raise SystemExit(label + ': codigo ' + str(result.returncode) + ' inesperado; evidencia: ' + str(log))
    return result


def project(folder, cpu):
    folder.mkdir()
    (folder / 'api.h').write_text(HEADER, encoding='utf-8')
    (folder / 'api.lib').write_bytes((root / 'lib' / 'api.lib').read_bytes())
    (folder / 'Main.ns').write_text(MAIN, encoding='utf-8')
    level = '<CpuLevel>' + cpu + '</CpuLevel>' if cpu else ''
    (folder / 'App.nsproj').write_text(
        '<Project><PropertyGroup><Name>T19NivelCpu</Name>' + level + '</PropertyGroup><ItemGroup>'
        '<Compile Include="Main.ns" /><CppLibraryReference Header="api.h" Language="C++" Standard="c++20" '
        'Library="api.lib" />' + CORE + '</ItemGroup></Project>', encoding='utf-8')
    return folder


def build(label, folder, extra=(), expected=0):
    return run(label, [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
                       '--no-artifact-cache', *extra, '-o', 'app.exe'], folder, expected)


library = root / 'lib'
library.mkdir()
(library / 'api.h').write_text(HEADER, encoding='utf-8')
(library / 'api.cpp').write_text(SOURCE, encoding='utf-8')
run('api', ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-march=x86-64-v3', '-O2', '-c',
            'api.cpp', '-o', 'api.obj'], library)
run('api-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], library)
expected = 1 * 1 + 2 * 2 + 3 * 3 + 4 * 4 + 5 * 5 + 6 * 6 + 7 * 7 + 8 * 8

option = project(root / 'opcion', None)
build('opcion', option, ['--cpu-level=x86-64-v3'])
code = run('opcion-run', [str(option / 'app.exe')], option, None).returncode
if code != expected:
    raise SystemExit(f'--cpu-level: el programa devolvio {code}, se esperaba {expected}; evidencia: {root}')
print(f'--cpu-level=x86-64-v3 sin CpuLevel en el proyecto: compila y devuelve {code}', flush=True)

override = project(root / 'prioridad', 'x86-64')
text = build('prioridad-base', override, expected=1).stdout
if 'x86-64-v3' not in text:
    raise SystemExit(f'CpuLevel x86-64 debia rechazar el vector de 256 bits; evidencia: {root}')
build('prioridad', override, ['--cpu-level=x86-64-v3'])
code = run('prioridad-run', [str(override / 'app.exe')], override, None).returncode
if code != expected:
    raise SystemExit(f'--cpu-level sobre CpuLevel: el programa devolvio {code}; evidencia: {root}')
print(f'CpuLevel x86-64 rechaza el vector de 256 bits; --cpu-level=x86-64-v3 tiene prioridad y devuelve {code}',
      flush=True)

for label, cpu, extra, message in (('proyecto-invalido', 'x86-64-v9', [], "CpuLevel must be"),
                                   ('opcion-invalida', None, ['--cpu-level=avx2'], 'CPU level must be')):
    folder = project(root / label, cpu)
    text = build(label, folder, extra, expected=1).stdout
    if message not in text or 'internal error' in text:
        print(text[-2000:], flush=True)
        raise SystemExit(f'{label}: se esperaba «{message}»; evidencia: {root}')
    print(f'{label}: rechazado con «{message}»', flush=True)

base = project(root / 'base-ir', None)
ir = run('base-ir', [args.nsc, '--dump-project-oop-llvm', 'App.nsproj', '--cpp-clang-root=' + args.clang_root], base,
         expected=1).stdout
if 'x86-64-v3' not in ir:
    raise SystemExit(f'sin nivel, el vector de 256 bits debia rechazarse con el nivel pedido; evidencia: {root}')
print('sin nivel, el vector de 256 bits se rechaza con el nivel que necesita', flush=True)
print('OK: nivel de CPU de la compilacion; evidencia:', root, flush=True)
