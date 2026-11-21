"""Funciones C que devuelven un puntero a funcion sin prototipo: `GetProcAddress` y `FARPROC`.

`FARPROC` es `INT_PTR (WINAPI *)()` en x64: un puntero a una funcion C sin prototipo. NSC rechazaba el retorno de
`GetProcAddress` ("C/C++ return type is not yet mapped") y el typedef ("C/C++ type alias cannot yet be represented
without losing type information") porque solo proyectaba punteros a funciones con prototipo. Ahora un puntero a una
funcion C sin prototipo, cdecl y sin parametros declarados comparte el tipo de funcion de `R (*)(void)`, la regla
que ya seguia una declaracion C directa sin prototipo; C hace compatibles esos dos tipos.

Casos, O0-O3, con windows.h del Windows SDK: resolver `GetCurrentProcessId` por nombre, convertir el `FARPROC` a un
puntero tipado y llamarlo, llamar el `FARPROC` sin argumentos, y un simbolo ausente devuelve null. Sin adaptadores
C ni conversion manual a `void*`.

Uso: python validar.py --nsc <nsc.exe> --clang-root <raiz Clang> --sdk-include <Include\\10.0.x>
     --sdk-lib <Lib\\10.0.x> --msvc-include <MSVC\\include> [--nslink <nslink.exe>]
"""
import argparse
import os
import subprocess
import tempfile
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
parser.add_argument('--sdk-include', required=True)
parser.add_argument('--sdk-lib', required=True)
parser.add_argument('--msvc-include', required=True)
parser.add_argument('--nslink')
args = parser.parse_args()
root = Path(tempfile.mkdtemp(prefix='nsc-farproc-', dir='A:/temp'))
env = dict(os.environ, TEMP=str(root), TMP=str(root))
if args.nslink:
    env['NSC_LINKER'] = args.nslink
print('Evidencia:', root, flush=True)

HEADER = '''#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef DWORD (WINAPI *ProcessIdFunction)(void);
'''
MAIN = '''void Fill(sbyte[] buffer, string text)
{
    for (int i = 0; i < text.Length; i++) buffer[i] = (sbyte)text[i];
    buffer[text.Length] = 0;
}

int Main()
{
    sbyte[] module = new sbyte[32];
    sbyte[] present = new sbyte[32];
    sbyte[] absent = new sbyte[32];
    Fill(module, "kernel32.dll");
    Fill(present, "GetCurrentProcessId");
    Fill(absent, "NoSuchExportNativeSharp");
    HMODULE kernel = GetModuleHandleA(&module[0]);
    if (kernel == null) return 1;
    FARPROC address = GetProcAddress(kernel, &present[0]);
    if (address == null) return 2;
    ProcessIdFunction function = (ProcessIdFunction)address;
    if (function() != GetCurrentProcessId()) return 3;
    if ((uint)address() != GetCurrentProcessId()) return 4;
    FARPROC missing = GetProcAddress(kernel, &absent[0]);
    if (missing != null) return 5;
    return 42;
}
'''


def run(label, command, cwd):
    result = subprocess.run(command, cwd=cwd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, errors='replace')
    (root / (label + '.log')).write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' +
                                         result.stdout, encoding='utf-8')
    return result


include = Path(args.sdk_include)
directories = ''.join(f'<IncludeDirectory Path="{path}" Kind="System" />'
                      for path in (include / 'um', include / 'shared', include / 'ucrt', Path(args.msvc_include)))
kernel32 = Path(args.sdk_lib) / 'um' / 'x64' / 'kernel32.lib'
for opt in range(4):
    case = root / f'O{opt}'
    case.mkdir()
    (case / 'win.h').write_text(HEADER, encoding='ascii')
    (case / 'Main.ns').write_text(MAIN, encoding='utf-8')
    (case / 'App.nsproj').write_text(
        '<Project><PropertyGroup><Name>FarProc</Name></PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
        f'<CppLibraryReference Header="win.h" Language="C" Standard="c17" Library="{kernel32}">' + directories +
        '</CppLibraryReference></ItemGroup></Project>', encoding='utf-8')
    build = run(f'O{opt}-nsc', [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
                                 '--no-artifact-cache', '-O' + str(opt), '-o', 'app.exe'], case)
    if build.returncode:
        print(build.stdout[-2000:], flush=True)
        raise SystemExit(f'O{opt}: nsc fallo; evidencia: {root}')
    code = run(f'O{opt}-run', [str(case / 'app.exe')], case).returncode
    if code != 42:
        raise SystemExit(f'O{opt}: devolvio {code}, se esperaba 42; evidencia: {root}')
    print(f'O{opt}: GetProcAddress, FARPROC tipado y sin argumentos, simbolo ausente null: 42', flush=True)
print('OK: FARPROC de GetProcAddress sin adaptadores, O0-O3; evidencia:', root)
