"""T17-007 y T17-008: informacion de depuracion en una llamada que cruza el limite de lenguaje.

`Main` NativeSharp llama a `A::one`, definida en la biblioteca, y a `A::two`, inline en el encabezado, que NSC
emite y que llama a `A::one`. La biblioteca se compila con `-g -gcodeview`. Con `-g` y con
`-gline-tables-only`, O0 y O2, y con NSLink y lld-link, el PDB del programa debe tener los archivos `Main.ns`
y `api.h` (y `api.cpp` si la biblioteca es estatica) y los procedimientos `Main`, `A::two` y, en estatica,
`A::one`, como el PDB del control C++ de Clang 23.1.0 tiene `control.cpp`, `api.h` y `api.cpp` y los mismos
procedimientos C++. El resultado es el mismo que el del control. Con --dll la biblioteca es una DLL con su
propio PDB, que tiene `api.cpp` y `A::one`.
"""
import argparse
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
root = Path(tempfile.mkdtemp(prefix='nsc-t17-depuracion-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)
CXX = ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-g', '-gcodeview']
HEADER = '''#pragma once
#if defined(T17_EXPORT)
#define API __declspec(dllexport)
#elif defined(T17_IMPORT)
#define API __declspec(dllimport)
#else
#define API
#endif
namespace A {
API int one(int x) noexcept;
inline int two(int x) noexcept
{
    return one(x) + 1;
}
}
'''
SOURCE = '#include "api.h"\nnamespace A {\nint one(int x) noexcept\n{\n    return x * 3;\n}\n}\n'
CONTROL = '#include "api.h"\nint main()\n{\n    int a = A::one(2);\n    return a + A::two(1);\n}\n'
NS_BODY = 'int a = A.one(2);\n    return a + A.two(1);'
EXPECTED = 10


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


def pdb(label, path, cwd):
    """Source file names and procedure names of a PDB. / Archivos fuente y procedimientos de un PDB."""
    files = run(label + '-files', ['llvm-pdbutil.exe', 'dump', '-files', str(path)], cwd).stdout
    names = {line.strip().replace('\\', '/').rsplit('/', 1)[-1] for line in files.splitlines()
             if line.strip().lower().endswith(('.ns', '.cpp', '.h'))}
    symbols = run(label + '-symbols', ['llvm-pdbutil.exe', 'dump', '-symbols', str(path)], cwd).stdout
    procedures = {line.split('`')[1] for line in symbols.splitlines() if 'S_GPROC32' in line and line.count('`') >= 2}
    return names, procedures


if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version'], root).stdout:
    raise SystemExit('Se requiere Clang 23.1.0')
for opt in (0, 2):
    for level in ('-g', '-gline-tables-only'):
        case = root / f'O{opt}{level}'
        case.mkdir()
        (case / 'api.h').write_text(HEADER, encoding='utf-8')
        (case / 'api.cpp').write_text(SOURCE, encoding='utf-8')
        (case / 'control.cpp').write_text(CONTROL, encoding='utf-8')
        flags = ['-O' + str(opt)]
        run(case.name + '-api', [*CXX, *flags, *(['-DT17_EXPORT'] if args.dll else []), '-c', 'api.cpp', '-o', 'api.obj'],
            case)
        if args.dll:
            run(case.name + '-dll', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib', '/debug',
                                     '/pdb:api.pdb', '/out:api.dll', '/implib:api.lib', 'api.obj'], case)
            (case / 'api.h').write_text('#define T17_IMPORT 1\n' + HEADER, encoding='utf-8')
        else:
            run(case.name + '-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], case)
        # The control at O0 keeps A::two out of line. / El control en O0 conserva A::two fuera de linea.
        run(case.name + '-control-obj', [*CXX, '-O0', '-c', 'control.cpp', '-o', 'control.obj'], case)
        run(case.name + '-control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console', '/nodefaultlib',
                                          '/debug', '/pdb:control.pdb', '/out:control.exe', 'control.obj', 'api.lib'], case)
        control = run(case.name + '-control-run', [str(case / 'control.exe')], case, expected=None).returncode
        if control != EXPECTED:
            raise SystemExit(f'{case.name}: el control C++ devolvio {control}; evidencia: {root}')
        control_files, control_procedures = pdb(case.name + '-control-pdb', case / 'control.pdb', case)
        cpp_files = {'api.h'} | (set() if args.dll else {'api.cpp'})
        cpp_procedures = {'A::two'} | (set() if args.dll else {'A::one'})
        if not cpp_files <= control_files or not cpp_procedures <= control_procedures:
            raise SystemExit(f'{case.name}: el PDB del control no tiene {cpp_files} y {cpp_procedures}: '
                             f'{control_files}, {control_procedures}; evidencia: {root}')
        (case / 'Main.ns').write_text('int Main()\n{\n    ' + NS_BODY + '\n}\n', encoding='utf-8')
        runtime = ' Runtime="api.dll"' if args.dll else ''
        (case / 'App.nsproj').write_text(
            '<Project><PropertyGroup><Name>T17Debug</Name></PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
            '<CppLibraryReference Header="api.h" Language="C++" Standard="c++20" Library="api.lib"' + runtime + ' />'
            '</ItemGroup></Project>', encoding='utf-8')
        for linker in ('nslink.exe', 'lld-link.exe'):
            tag = f'{case.name}-{linker.split(".")[0]}'
            exe = 'app-' + linker.split('.')[0].split('-')[0] + '.exe'
            run(tag + '-nsc', [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
                               '--no-artifact-cache', '-O' + str(opt), level, '-o', exe], case,
                extra_env={'NSC_LINKER': linker})
            result = run(tag + '-run', [str(case / exe)], case, expected=None).returncode
            if result != EXPECTED:
                raise SystemExit(f'{tag}: NativeSharp devolvio {result}, Clang {EXPECTED}; evidencia: {root}')
            files, procedures = pdb(tag + '-pdb', case / exe.replace('.exe', '.pdb'), case)
            if not ({'Main.ns'} | cpp_files) <= files or not ({'Main'} | cpp_procedures) <= procedures:
                raise SystemExit(f'{tag}: el PDB tiene {sorted(files)} y {sorted(procedures)}; se esperaba '
                                 f'Main.ns, {sorted(cpp_files)}, Main y {sorted(cpp_procedures)}; evidencia: {root}')
            print(f'{tag}: {result} como Clang; PDB con {sorted(files & ({"Main.ns"} | cpp_files))} y '
                  f'{sorted(procedures & ({"Main"} | cpp_procedures))}', flush=True)
        if args.dll:
            files, procedures = pdb(case.name + '-api-pdb', case / 'api.pdb', case)
            if 'api.cpp' not in files or 'A::one' not in procedures:
                raise SystemExit(f'{case.name}: el PDB de la DLL no tiene api.cpp y A::one; evidencia: {root}')
print('OK: informacion de depuracion de NativeSharp, del encabezado y de la biblioteca, frente a Clang; evidencia:', root)
