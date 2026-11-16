"""T18-010 y T18-011: ICU4C 80 (`icuuc`), una biblioteca C++ real con compilacion separada.

Las 203 unidades de `icu4c/source/common` y `stubdata` se compilan con Clang 23.1.0 (`-std=c++17 -O2`) en una
biblioteca estatica o, con --dll, en una DLL que exporta `U_COMMON_API`. NativeSharp usa tres clases con vida no
trivial, destructor virtual y constructores sin `noexcept`: `icu::ParsePosition` (indices), `icu::Edits` (una
lista de cambios que crece en memoria propia de la biblioteca y se libera en su destructor) e `icu::ErrorCode`.
El programa NativeSharp y un control C++ de Clang hacen las mismas llamadas y devuelven el mismo valor, O0-O3; el
bucle crea y destruye 200 `Edits` con 600 cambios cada uno. El ejecutable estatico arranca por el runtime de C
(`--crt-startup`).
"""
import argparse
import concurrent.futures
import os
import shutil
import subprocess
import tempfile
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
parser.add_argument('--dll', action='store_true')
args = parser.parse_args()
root = Path(tempfile.mkdtemp(prefix='nsc-t18-icu-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)
ICU = Path('A:/al82dev-clang-deps/icu/icu4c/source')
TOOLSET = Path('A:/vc2026-runtime/toolsets/14.52.36815')
SDK = Path('A:/vc2026-runtime/windows-sdk/10.0.28000.0')
CRT = [TOOLSET / 'lib/x64/libcmt.lib', TOOLSET / 'lib/x64/libvcruntime.lib', TOOLSET / 'lib/x64/libcpmt.lib',
       SDK / 'lib/ucrt/x64/libucrt.lib', SDK / 'lib/um/x64/kernel32.lib', SDK / 'lib/um/x64/advapi32.lib',
       SDK / 'lib/um/x64/user32.lib']
SYSTEM = [TOOLSET / 'include', SDK / 'include/ucrt', SDK / 'include/um', SDK / 'include/shared']
CXX = ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++17', *[x for p in SYSTEM for x in ('-isystem', str(p))],
       '-I', str(ICU / 'common')]
LINKAGE = [] if args.dll else ['-DU_STATIC_IMPLEMENTATION']
HEADER = '#pragma once\n#include "unicode/parsepos.h"\n#include "unicode/edits.h"\n#include "unicode/errorcode.h"\n'
NS = '''int Positions()
{
    icu_80.ParsePosition p = new icu_80.ParsePosition(5);
    int a = p.getIndex();
    p.setIndex(9);
    p.setErrorIndex(3);
    return a * 100 + p.getIndex() * 10 + p.getErrorIndex();
}
int Changes()
{
    icu_80.Edits e = new icu_80.Edits();
    for (int i = 0; i < 500; i = i + 1)
    {
        e.addReplace(1, 2);
        e.addUnchanged(3);
    }
    return e.lengthDelta() + e.numberOfChanges() * 1000 + (e.hasChanges() != 0 ? 7 : 0);
}
int Repeated()
{
    int total = 0;
    for (int k = 0; k < 200; k = k + 1)
    {
        icu_80.Edits e = new icu_80.Edits();
        for (int i = 0; i < 300; i = i + 1)
        {
            e.addReplace(2, 1);
            e.addUnchanged(1);
        }
        total = total + e.lengthDelta();
    }
    return total % 251;
}
int Codes()
{
    icu_80.ErrorCode c = new icu_80.ErrorCode();
    return c.isSuccess() != 0 ? 4 : 5;
}
int Main()
{
    return Positions() * 7 + Changes() * 3 + Repeated() * 11 + Codes();
}
'''
CONTROL = '''#include "api.h"
static int Positions() { icu::ParsePosition p(5); int a = p.getIndex(); p.setIndex(9); p.setErrorIndex(3);
    return a * 100 + p.getIndex() * 10 + p.getErrorIndex(); }
static int Changes() { icu::Edits e; for (int i = 0; i < 500; i = i + 1) { e.addReplace(1, 2); e.addUnchanged(3); }
    return e.lengthDelta() + e.numberOfChanges() * 1000 + (e.hasChanges() != 0 ? 7 : 0); }
static int Repeated() { int total = 0; for (int k = 0; k < 200; k = k + 1) { icu::Edits e;
    for (int i = 0; i < 300; i = i + 1) { e.addReplace(2, 1); e.addUnchanged(1); } total = total + e.lengthDelta(); }
    return total % 251; }
static int Codes() { icu::ErrorCode c; return c.isSuccess() != 0 ? 4 : 5; }
int main() { return Positions() * 7 + Changes() * 3 + Repeated() * 11 + Codes(); }
'''


def run(label, command, cwd, expected=0, extra_env=None):
    result = subprocess.run(command, cwd=cwd, env={**env, **(extra_env or {})}, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    log = root / (label + '.log')
    log.write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' + result.stdout,
                   encoding='utf-8')
    if expected is not None and ((expected == 0 and result.returncode) or (expected != 0 and result.returncode == 0)):
        print(result.stdout[-3000:], flush=True)
        raise SystemExit(label + ': codigo ' + str(result.returncode) + ' inesperado; evidencia: ' + str(log))
    return result


if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version'], root).stdout:
    raise SystemExit('Se requiere Clang 23.1.0')
library = root / 'icuuc'
library.mkdir()
sources = sorted((ICU / 'common').glob('*.cpp')) + sorted((ICU / 'common').glob('*.c')) + [ICU / 'stubdata' / 'stubdata.cpp']
flags = ['-O2', '-DU_COMMON_IMPLEMENTATION', '-DU_ATTRIBUTE_DEPRECATED=', '-DWIN32', '-D_WINDOWS', '-DNOMINMAX', *LINKAGE]


def compile_unit(source):
    name = source.stem + ('-stub' if source.parent.name == 'stubdata' else '')
    command = [*CXX, *flags, '-c', str(source), '-o', name + '.obj'] if source.suffix == '.cpp' else \
        ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c11', *CXX[4:], *flags, '-c', str(source), '-o', name + '.obj']
    result = subprocess.run(command, cwd=library, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                            errors='replace')
    return name + '.obj', result.returncode, result.stdout


with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
    built = list(pool.map(compile_unit, sources))
failed = [(name, output) for name, code, output in built if code]
if failed:
    raise SystemExit(f'ICU: {len(failed)} unidades no compilan ({failed[0][0]}): {failed[0][1][-800:]}')
(library / 'objetos.rsp').write_text('\n'.join(name for name, _, _ in built), encoding='utf-8')
if args.dll:
    run('icuuc-dll', ['lld-link.exe', '/nologo', '/dll', '/out:icuuc80.dll', '/implib:icuuc.lib', '@objetos.rsp',
                      *[str(p) for p in CRT]], library)
else:
    run('icuuc-lib', ['llvm-lib.exe', '/nologo', '/out:icuuc.lib', '@objetos.rsp'], library)
print(f'icuuc: {len(sources)} unidades compiladas con Clang 23.1.0', flush=True)
system = ''.join('<IncludeDirectory Path="' + str(p).replace('\\', '/') + '" Kind="System" />' for p in SYSTEM)
system += '<IncludeDirectory Path="' + str(ICU / 'common').replace('\\', '/') + '" />'
system += ''.join('<Define Value="' + item[2:] + '" />' for item in LINKAGE)
for opt in range(4):
    case = root / f'O{opt}'
    case.mkdir()
    for name in ('icuuc.lib', *(('icuuc80.dll',) if args.dll else ())):
        shutil.copyfile(library / name, case / name)
    (case / 'api.h').write_text(HEADER, encoding='utf-8')
    (case / 'control.cpp').write_text(CONTROL, encoding='utf-8')
    run(f'O{opt}-control-obj', [*CXX, '-O' + str(opt), *LINKAGE, '-I.', '-c', 'control.cpp', '-o', 'control.obj'], case)
    run(f'O{opt}-control-link', ['lld-link.exe', '/nologo', '/subsystem:console', '/out:control.exe', 'control.obj',
                                 'icuuc.lib', *[str(p) for p in CRT]], case)
    control = run(f'O{opt}-control-run', [str(case / 'control.exe')], case, expected=None).returncode
    (case / 'Main.ns').write_text(NS, encoding='utf-8')
    runtime = ' Runtime="icuuc80.dll"' if args.dll else ''
    (case / 'App.nsproj').write_text(
        '<Project><PropertyGroup><Name>T18Icu</Name></PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
        '<CppLibraryReference Header="api.h" Language="C++" Standard="c++17" Library="icuuc.lib"' + runtime + '>' +
        system + '</CppLibraryReference></ItemGroup></Project>', encoding='utf-8')
    command = [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
               '--no-artifact-cache', '-O' + str(opt)]
    extra = {'NSC_LINKER': 'nslink.exe'}
    if not args.dll:
        command.append('--crt-startup')
        for path in CRT:
            command += ['--link-lib', str(path)]
        extra = {'NSC_LINKER': 'lld-link.exe'}
    run(f'O{opt}-nsc', command + ['-o', 'app.exe'], case, extra_env=extra)
    result = run(f'O{opt}-run', [str(case / 'app.exe')], case, expected=None).returncode
    if result != control:
        raise SystemExit(f'O{opt}: NativeSharp devolvio {result}, Clang {control}; evidencia: {root}')
    print(f'O{opt}: NativeSharp y Clang devuelven {result} (ParsePosition, Edits, ErrorCode)', flush=True)
print('OK: ICU4C icuuc consumida desde NativeSharp, frente a Clang; evidencia:', root)
