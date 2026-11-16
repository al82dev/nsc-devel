"""T18-017 y T18-018: paquete de distribucion de NSC probado en un entorno limpio.

Instala el build con `cmake --install` en un prefijo de A:/temp, agrega NSLink y lld-link junto a nsc.exe y
copia solo los encabezados de recursos de Clang 23.1.0. Despues compila y ejecuta, con un entorno que solo
tiene SystemRoot, TEMP y PATH=System32, un programa NativeSharp solo, una biblioteca C estatica, una biblioteca
C++ que usa la STL con el runtime de C, una DLL NativeSharp, una biblioteca estatica NativeSharp y tres negativos
(sin recursos de Clang, sin NSLink, sin llvm-lib).
"""
import argparse
import os
import re
import shutil
import subprocess
import tempfile
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
args = parser.parse_args()
# The build tree that holds nsc.exe and its install rules. / El arbol de build con nsc.exe y sus reglas de instalacion.
BUILD = Path(args.nsc).resolve().parent.parent
CLANG = Path(args.clang_root)
TOOLSET = Path('A:/vc2026-runtime/toolsets/14.52.36815')
SDK = Path('A:/vc2026-runtime/windows-sdk/10.0.28000.0')
CRT = [TOOLSET / 'lib/x64/libcmt.lib', TOOLSET / 'lib/x64/libvcruntime.lib', TOOLSET / 'lib/x64/libcpmt.lib',
       SDK / 'lib/ucrt/x64/libucrt.lib', SDK / 'lib/um/x64/kernel32.lib']
SYSTEM = [TOOLSET / 'include', SDK / 'include/ucrt', SDK / 'include/um', SDK / 'include/shared']

root = Path(tempfile.mkdtemp(prefix='nsc-t18-paquete-', dir='A:/temp'))
pkg = root / 'pkg'
resources = root / 'clang-recursos'
tmp = root / 'tmp'
tmp.mkdir()
print('Evidencia:', root, flush=True)
if 'clang version 23.1.0' not in subprocess.run([str(Path(args.clang_root) / 'bin/clang.exe'), '--version'],
                                                stdout=subprocess.PIPE, text=True).stdout:
    raise SystemExit('Se requiere Clang 23.1.0')
host = os.environ.copy()
host.pop('NSC_LINKER', None)
host.pop('NSC_WINDOWS_RUNTIME_LIBRARY', None)
host['PATH'] = str(CLANG / 'bin') + os.pathsep + host.get('PATH', '')
clean = {'SystemRoot': os.environ['SystemRoot'], 'SystemDrive': os.environ.get('SystemDrive', 'C:'),
         'PATH': os.environ['SystemRoot'] + '\\System32', 'TEMP': str(tmp), 'TMP': str(tmp)}


def run(label, command, cwd, expected=0, environment=None):
    result = subprocess.run([str(c) for c in command], cwd=cwd, env=environment or host, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    log = root / (label + '.log')
    log.write_text('COMMAND: ' + repr(command) + '\nENV: ' + repr(sorted((environment or host).keys())) +
                   '\nEXIT: ' + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
    if expected is not None and ((expected == 0 and result.returncode) or (expected != 0 and result.returncode == 0)):
        print(result.stdout[-3000:], flush=True)
        raise SystemExit(label + ': codigo ' + str(result.returncode) + ' inesperado; evidencia: ' + str(log))
    return result


# Package. / Paquete.
cache = (BUILD / 'CMakeCache.txt').read_text(encoding='utf-8', errors='replace')
cmake = re.search(r'^CMAKE_COMMAND:INTERNAL=(.+)$', cache, re.M).group(1).strip()
prefix = re.search(r'^CMAKE_INSTALL_PREFIX:PATH=(.+)$', cache, re.M).group(1).strip()
print('cmake:', cmake, '| prefijo configurado:', prefix, flush=True)
run('instalar', [cmake, '--install', BUILD, '--prefix', pkg], root)
# NSLink is its own product; the installed one is copied, not modified. / NSLink es otro producto; se copia el
# instalado, sin modificarlo.
shutil.copy2(Path('C:/native-sharp/bin/nslink.exe'), pkg / 'bin/nslink.exe')
shutil.copy2(CLANG / 'bin/lld-link.exe', pkg / 'bin/lld-link.exe')
shutil.copytree(CLANG / 'lib/clang/23/include', resources / 'lib/clang/23/include')
files = sorted(str(p.relative_to(pkg)).replace('\\', '/') for p in pkg.rglob('*') if p.is_file())
(root / 'paquete-archivos.txt').write_text('\n'.join(files) + '\n', encoding='utf-8')
size = sum((pkg / f).stat().st_size for f in files)
print(f'paquete: {len(files)} archivos, {size // 1024} KiB; recursos de Clang: '
      f'{sum(1 for p in resources.rglob("*") if p.is_file())} encabezados', flush=True)
for tool in ('nsc.exe', 'nslink.exe', 'lld-link.exe'):
    out = run('imports-' + tool, ['llvm-readobj.exe', '--coff-imports', pkg / 'bin' / tool], root).stdout
    dlls = sorted(set(re.findall(r'Name: (\S+\.dll)', out, re.I)))
    print(f'{tool}: importa {", ".join(dlls)}', flush=True)
nsc = pkg / 'bin/nsc.exe'
version = run('version', [nsc, '--version'], root, environment=clean).stdout.strip().splitlines()
print('nsc --version:', version[0] if version else '', flush=True)

system = ''.join('<IncludeDirectory Path="' + str(p).replace('\\', '/') + '" Kind="System" />' for p in SYSTEM)
CXX = ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-O2',
       *[x for p in SYSTEM for x in ('-isystem', str(p))]]
CC = ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c17', '-O2']


def project(case, name, body, reference='', output_type=''):
    (case / 'Main.ns').write_text(body, encoding='utf-8')
    kind = f'<OutputType>{output_type}</OutputType>' if output_type else ''
    (case / 'App.nsproj').write_text(
        f'<Project><PropertyGroup><Name>{name}</Name>{kind}</PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
        + reference + '</ItemGroup></Project>', encoding='utf-8')


def check(label, case, exe, expected):
    result = run(label + '-run', [case / exe], case, expected=None, environment=clean).returncode
    if result != expected:
        raise SystemExit(f'{label}: devolvio {result}, se esperaba {expected}; evidencia: {root}')
    print(f'{label}: {result}', flush=True)


for opt in ('-O0', '-O2'):
    tag = opt[1:]
    # NativeSharp only. / Solo NativeSharp.
    case = root / ('ns-' + tag)
    case.mkdir()
    project(case, 'T18Solo', 'int Twice(int x) { return x * 2; }\nint Main() { return Twice(21); }\n')
    run('ns-' + tag + '-nsc', [nsc, '--emit-project-oop-exe', 'App.nsproj', '--no-artifact-cache', opt,
                               '-o', 'app.exe'], case, environment=clean)
    check('ns-' + tag, case, 'app.exe', 42)
    # Static C library built by its vendor. / Biblioteca C estatica construida por su proveedor.
    case = root / ('c-' + tag)
    case.mkdir()
    (case / 'calc.h').write_text('#pragma once\ntypedef struct calc_pair { int a; int b; } calc_pair;\n'
                                 'int calc_mul(calc_pair p);\n', encoding='utf-8')
    (case / 'calc.c').write_text('#include "calc.h"\nint calc_mul(calc_pair p) { return p.a * p.b; }\n',
                                 encoding='utf-8')
    run('c-' + tag + '-cc', [*CC, '-c', 'calc.c', '-o', 'calc.obj'], case)
    run('c-' + tag + '-lib', ['llvm-lib.exe', '/nologo', '/out:calc.lib', 'calc.obj'], case)
    project(case, 'T18C', 'int Main() { calc_pair p; p.a = 6; p.b = 7; return calc_mul(p); }\n',
            '<CppLibraryReference Header="calc.h" Language="C" Standard="c17" Library="calc.lib" />')
    run('c-' + tag + '-nsc', [nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + str(resources),
                              '--no-artifact-cache', opt, '-o', 'app.exe'], case, environment=clean)
    check('c-' + tag, case, 'app.exe', 42)
    # C++ library with the STL and the C runtime. / Biblioteca C++ con la STL y el runtime de C.
    case = root / ('cpp-' + tag)
    case.mkdir()
    (case / 'api.h').write_text('#pragma once\n#include <numeric>\nnamespace T18P { int twice(int x); }\n',
                                encoding='utf-8')
    (case / 'api.cpp').write_text('#include "api.h"\n#include <vector>\nnamespace T18P { int twice(int x) {\n'
                                  '  std::vector<int> v; v.push_back(x); v.push_back(x);\n'
                                  '  return std::accumulate(v.begin(), v.end(), 0); } }\n', encoding='utf-8')
    run('cpp-' + tag + '-cxx', [*CXX, '-c', 'api.cpp', '-o', 'api.obj'], case)
    run('cpp-' + tag + '-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], case)
    project(case, 'T18Cpp', 'int Main() { return std.gcd<int, int>(84, 36) + T18P.twice(15); }\n',
            '<CppLibraryReference Header="api.h" Language="C++" Standard="c++20" Library="api.lib">' + system +
            '</CppLibraryReference>')
    crt = ['--crt-startup'] + [x for p in CRT for x in ('--link-lib', p)]
    run('cpp-' + tag + '-nsc', [nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + str(resources),
                                '--no-artifact-cache', opt, *crt, '-o', 'app.exe'], case, environment=clean)
    check('cpp-' + tag, case, 'app.exe', 42)
    # NativeSharp DLL loaded by a C program. / DLL NativeSharp cargada por un programa C.
    case = root / ('dll-' + tag)
    case.mkdir()
    project(case, 'T18Dll', 'namespace P18 { public class Lib { public static int Get() { return 42; } } }\n',
            output_type='SharedLibrary')
    run('dll-' + tag + '-nsc', [nsc, '--emit-project-oop-dll', 'App.nsproj', '--no-artifact-cache', opt,
                                '-o', 'lib.dll'], case, environment=clean)
    exports = run('dll-' + tag + '-exports', ['llvm-readobj.exe', '--coff-exports', case / 'lib.dll'], case).stdout
    names = [n for n in re.findall(r'Name: (\S+)', exports) if 'Get' in n]
    if not names:
        raise SystemExit(f'dll-{tag}: la DLL no exporta Get; evidencia: {root}')
    (case / 'host.c').write_text('#include <windows.h>\nint main(void) { HMODULE m = LoadLibraryA("lib.dll");\n'
                                 '  if (!m) return 1; FARPROC f = GetProcAddress(m, "' + names[0] + '");\n'
                                 '  if (!f) return 2; return ((int (*)(void))f)(); }\n', encoding='utf-8')
    run('dll-' + tag + '-host', [*CC, *[x for p in SYSTEM for x in ('-isystem', str(p))], '-c', 'host.c',
                                 '-o', 'host.obj'], case)
    run('dll-' + tag + '-host-link', ['lld-link.exe', '/nologo', '/subsystem:console', '/out:host.exe', 'host.obj',
                                      *CRT], case)
    check('dll-' + tag + ' (' + names[0] + ')', case, 'host.exe', 42)

# Negatives. / Negativos.
case = root / 'c-O2'
out = run('neg-sin-recursos', [nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + str(root / 'nada'),
                               '--no-artifact-cache', '-o', 'x.exe'], case, expected=1, environment=clean).stdout
if 'Clang resource include directory is not accessible' not in out:
    print(out[-2000:])
    raise SystemExit('neg-sin-recursos: falta el diagnostico; evidencia: ' + str(root))
print('negativo sin recursos de Clang: "Clang resource include directory is not accessible"', flush=True)
bare = root / 'pkg-sin-nslink'
shutil.copytree(pkg, bare)
(bare / 'bin/nslink.exe').unlink()
case = root / 'ns-O0'
out = run('neg-sin-nslink', [bare / 'bin/nsc.exe', '--emit-project-oop-exe', 'App.nsproj', '--no-artifact-cache',
                             '-o', 'x.exe'], case, expected=1, environment=clean).stdout
if 'NSLink was not found beside nsc.exe or in PATH' not in out:
    print(out[-2000:])
    raise SystemExit('neg-sin-nslink: falta el diagnostico; evidencia: ' + str(root))
print('negativo sin NSLink: "NSLink was not found beside nsc.exe or in PATH"', flush=True)
shutil.rmtree(bare)
# A static library needs llvm-lib beside the linker or in PATH. / Una biblioteca estatica necesita llvm-lib junto al
# enlazador o en PATH.
case = root / 'lib'
case.mkdir()
project(case, 'T18Lib', 'namespace P18 { public class Lib { public static int Get() { return 42; } } }\n',
        output_type='StaticLibrary')
out = run('neg-sin-llvm-lib', [nsc, '--emit-project-oop-lib', 'App.nsproj', '--no-artifact-cache', '-o', 'p18.lib'],
          case, expected=1, environment=clean).stdout
if 'llvm-lib' not in out:
    print(out[-2000:])
    raise SystemExit('neg-sin-llvm-lib: falta el diagnostico; evidencia: ' + str(root))
print('negativo sin llvm-lib: la biblioteca estatica nombra llvm-lib', flush=True)
shutil.copy2(CLANG / 'bin/llvm-lib.exe', pkg / 'bin/llvm-lib.exe')
run('lib-nsc', [nsc, '--emit-project-oop-lib', 'App.nsproj', '--no-artifact-cache', '-o', 'p18.lib'], case,
    environment=clean)
members = run('lib-list', ['llvm-lib.exe', '/list', case / 'p18.lib'], case).stdout.split()
if not members:
    raise SystemExit('lib: la biblioteca estatica no tiene miembros; evidencia: ' + str(root))
print(f'biblioteca estatica con llvm-lib junto a nsc.exe: {len(members)} miembro(s)', flush=True)
print('OK: paquete instalado en un prefijo nuevo y probado en un entorno limpio; evidencia:', root)
