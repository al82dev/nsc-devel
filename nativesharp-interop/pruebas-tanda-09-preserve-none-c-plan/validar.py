"""T09-003: firmas C preserve_none admitidas por el plan ABI de Clang.

Compara un control C compilado por Clang 23.1.0 con un consumidor NativeSharp
que llama directamente cinco funciones C preserve_none sin records por valor, y
dos de ellas por punteros a funcion preserve_none devueltos por C, en biblioteca estatica y DLL, O0-O3. Con --precompilado tambien publica una
plantilla NativeSharp que llama las mismas funciones y la consume con y sin
encabezado. Exige la convencion preserve_nonecc y el simbolo C en el IR de NSC
y conserva dos rechazos que Clang acepta: una firma de 33 parametros, fuera del
validador compartido (diagnostico HIR), y una funcion variadica (diagnostico de importacion).
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
parser.add_argument('--dll', action='store_true')
parser.add_argument('--precompilado', action='store_true')
args = parser.parse_args()
root = Path(tempfile.mkdtemp(prefix='nsc-t09-preserve-none-c-plan-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)

HEADER = '''#pragma once
#if defined(T09_EXPORT)
#define API __declspec(dllexport)
#elif defined(T09_IMPORT)
#define API __declspec(dllimport)
#else
#define API
#endif
#define PN __attribute__((preserve_none))
API int PN t09_pn_pair(int a, int b);
API double PN t09_pn_mixed(float a, int b, double c);
API void PN t09_pn_store(int *target, long long value);
API unsigned short PN t09_pn_narrow(_Bool flag, signed char low, unsigned char high);
API int *PN t09_pn_offset(int *base, int index);
typedef int (PN *T09PnPair)(int, int);
typedef double (PN *T09PnMixed)(float, int, double);
API T09PnPair t09_pn_get_pair(void);
API T09PnMixed t09_pn_get_mixed(void);
'''
SOURCE = '''#include "api.h"
__declspec(selectany) int _fltused = 0;
int PN t09_pn_pair(int a, int b) { return a * 10 + b; }
double PN t09_pn_mixed(float a, int b, double c) { return a + b * 2 + c * 4; }
void PN t09_pn_store(int *target, long long value) { *target = (int)(value - 7); }
unsigned short PN t09_pn_narrow(_Bool flag, signed char low, unsigned char high)
{ return (unsigned short)((flag ? 1000 : 0) + low + high); }
int *PN t09_pn_offset(int *base, int index) { return base + index; }
T09PnPair t09_pn_get_pair(void) { return t09_pn_pair; }
T09PnMixed t09_pn_get_mixed(void) { return t09_pn_mixed; }
'''
CONTROL = '''#include "api.h"
__declspec(selectany) int _fltused = 0;
int main(void)
{
    int values[3] = {4, 5, 6}; int stored = 0;
    if (t09_pn_pair(2, 3) != 23) return 1;
    if (t09_pn_mixed(0.5f, 3, 1.25) != 11.5) return 2;
    t09_pn_store(&stored, 50); if (stored != 43) return 3;
    if (t09_pn_narrow(1, -3, 200) != 1197) return 4;
    if (*t09_pn_offset(values, 2) != 6) return 5;
    if (t09_pn_get_pair()(4, 5) != 45 || t09_pn_get_mixed()(1.0f, 1, 1.0) != 7.0) return 6;
    return 0;
}
'''
BODY = ('int stored = 0; '
        'if (t09_pn_pair(2, 3) != 23) return 11; '
        'if (t09_pn_mixed(0.5f, 3, 1.25) != 11.5) return 12; '
        't09_pn_store(&stored, (long)50); if (stored != 43) return 13; '
        'if (t09_pn_narrow(true, (sbyte)(-3), (byte)200) != 1197) return 14; '
        'int local = 9; if (*t09_pn_offset(&local, 0) != 9) return 15; '
        'T09PnPair pair = t09_pn_get_pair(); if (pair(4, 5) != 45) return 16; '
        'T09PnMixed mixed = t09_pn_get_mixed(); if (mixed(1.0f, 1, 1.0) != 7.0) return 17; ')
SYMBOLS = ('t09_pn_pair', 't09_pn_mixed', 't09_pn_store', 't09_pn_narrow', 't09_pn_offset')


def run(label, command, cwd, expected=0):
    result = subprocess.run(command, cwd=cwd, env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    log = root / (label + '.log')
    log.write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' + result.stdout,
                   encoding='utf-8')
    if (expected == 0 and result.returncode) or (expected != 0 and result.returncode == 0):
        print(result.stdout, flush=True)
        raise SystemExit(label + ': codigo inesperado; evidencia: ' + str(log))
    return result.stdout


def project(name, references):
    return ('<Project><PropertyGroup><Name>' + name + '</Name></PropertyGroup><ItemGroup>'
            '<Compile Include="Main.ns" />' + references + '</ItemGroup></Project>')


def c_reference(prefix=''):
    runtime = ' Runtime="' + prefix + 'api.dll"' if args.dll else ''
    return ('<CppLibraryReference Header="' + prefix + 'api.h" Language="C" Standard="c17" Library="'
            + prefix + 'api.lib"' + runtime + ' />')


if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version'], root):
    raise SystemExit('Se requiere Clang 23.1.0')
for opt in range(4):
    case = root / ('O' + str(opt)); case.mkdir()
    (case / 'api.h').write_text(HEADER, encoding='utf-8')
    (case / 'api.c').write_text(SOURCE, encoding='utf-8')
    (case / 'Control.c').write_text(CONTROL, encoding='utf-8')
    flags = ['-target', 'x86_64-pc-windows-msvc', '-std=c17', '-O' + str(opt)]
    run(f'O{opt}-api', ['clang.exe', *flags, *(['-DT09_EXPORT'] if args.dll else []), '-c', 'api.c',
                         '-o', 'api.obj'], case)
    clang_ir = run(f'O{opt}-api-ir', ['clang.exe', *flags, '-O0', '-S', '-emit-llvm', 'api.c', '-o', '-'], case)
    for symbol in SYMBOLS:
        if not re.search(r'define dso_local preserve_nonecc [^@]*@' + symbol + r'\(', clang_ir):
            raise SystemExit(f'O{opt}: Clang no emite preserve_nonecc para {symbol}; evidencia: {root}')
    if args.dll:
        run(f'O{opt}-lib', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib', '/out:api.dll',
                             '/implib:api.lib', 'api.obj'], case)
        path = case / 'api.h'; path.write_text('#define T09_IMPORT 1\n' + path.read_text(encoding='utf-8'),
                                               encoding='utf-8')
    else:
        run(f'O{opt}-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], case)
    run(f'O{opt}-control-obj', ['clang.exe', *flags, '-c', 'Control.c', '-o', 'control.obj'], case)
    run(f'O{opt}-control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console',
                                  '/nodefaultlib', '/out:control.exe', 'control.obj', 'api.lib'], case)
    run(f'O{opt}-control-run', [str(case / 'control.exe')], case)
    (case / 'Main.ns').write_text('int Main() { ' + BODY + 'return 0; }\n', encoding='utf-8')
    (case / 'Fixture.nsproj').write_text(project('Fixture', c_reference()), encoding='utf-8')
    common = ['Fixture.nsproj', '--cpp-clang-root=' + args.clang_root]
    ir = run(f'O{opt}-ir', [args.nsc, '--dump-project-oop-llvm', *common], case)
    for symbol in SYMBOLS:
        declaration = 'declare ' + ('dllimport ' if args.dll else '') + 'preserve_nonecc '
        if not re.search(r'call preserve_nonecc [^@]*@' + symbol + r'\(', ir) or \
                not any(line.startswith(declaration) and '@' + symbol + '(' in line for line in ir.splitlines()):
            raise SystemExit(f'O{opt}: convencion o simbolo NSC distinto para {symbol}; evidencia: {root}')
    for result in ('i32', 'double'):
        if not re.search(r'call preserve_nonecc (?:noundef )?' + result + r' %[\w.]+\(', ir):
            raise SystemExit(f'O{opt}: llamada por puntero preserve_nonecc {result} ausente; evidencia: {root}')
    run(f'O{opt}-build', [args.nsc, '--emit-project-oop-exe', *common, '--no-artifact-cache', '-O' + str(opt),
                           '-o', 'native.exe'], case)
    run(f'O{opt}-native-run', [str(case / 'native.exe')], case)
    print(f'O{opt}: control C y NativeSharp directo pasaron', flush=True)
    if args.precompilado:
        producer = case / 'producer'; producer.mkdir()
        for name in ('api.h', 'api.lib') + (('api.dll',) if args.dll else ()):
            shutil.copyfile(case / name, producer / name)
        (producer / 'Producer.ns').write_text('namespace T09 { public class Bridge { public static int Run<T>(T unused) { '
            + BODY + 'return 0; } } }\n', encoding='utf-8')
        (producer / 'Producer.nsproj').write_text('<Project><PropertyGroup><Name>T09Producer</Name>'
            '<OutputType>Library</OutputType></PropertyGroup><ItemGroup><Compile Include="Producer.ns" />'
            + c_reference() + '</ItemGroup></Project>', encoding='utf-8')
        run(f'O{opt}-producer', [args.nsc, '--emit-project-oop-lib', 'Producer.nsproj', '--cpp-clang-root='
            + args.clang_root, '--no-artifact-cache', '-O' + str(opt), '-o', 'Producer.lib'], producer)
        for visibility in ('con-encabezado', 'sin-encabezado'):
            consumer = case / visibility; consumer.mkdir()
            references = '<LibraryReference Include="../producer/Producer.lib" />'
            if visibility == 'con-encabezado':
                references += c_reference('../producer/')
            else:
                (producer / 'api.h').rename(producer / 'api.h.oculto')
            (consumer / 'Main.ns').write_text('int Main() { return T09.Bridge.Run<int>(0); }\n', encoding='utf-8')
            (consumer / 'Consumer.nsproj').write_text(project('T09Consumer', references), encoding='utf-8')
            run(f'O{opt}-{visibility}-build', [args.nsc, '--emit-project-oop-exe', 'Consumer.nsproj',
                '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O' + str(opt), '-o',
                'consumer.exe'], consumer)
            if args.dll: shutil.copyfile(producer / 'api.dll', consumer / 'api.dll')
            run(f'O{opt}-{visibility}-run', [str(consumer / 'consumer.exe')], consumer)
            print(f'O{opt}: plantilla precompilada {visibility} paso', flush=True)
    if opt == 0:
        # C records in preserve_none signatures are now admitted through Clang plans, so the rejection
        # control keeps its HIR check with a signature that no shared preserve_none validator covers:
        # 33 int parameters (the plan validators are bounded to 32). Clang must accept the same declaration.
        many = ', '.join(f'int a{i}' for i in range(33))
        negative = root / 'O0-33-parametros'; negative.mkdir()
        shutil.copyfile(case / 'api.lib', negative / 'api.lib')
        if args.dll: shutil.copyfile(case / 'api.dll', negative / 'api.dll')
        (negative / 'api.h').write_text((case / 'api.h').read_text(encoding='utf-8') +
            f'API int PN t09_pn_many({many});\n', encoding='utf-8')
        (negative / 'many.c').write_text('#include "api.h"\nint PN t09_pn_many(' + many + ') '
            '{ return a0 + a32; }\n', encoding='utf-8')
        run('O0-33-parametros-clang', ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c17', '-O0',
            *(['-DT09_EXPORT'] if args.dll else []), '-c', 'many.c', '-o', 'many.obj'], negative)
        call = ', '.join(str(i) for i in range(33))
        (negative / 'Main.ns').write_text(f'int Main() {{ return t09_pn_many({call}); }}\n', encoding='utf-8')
        (negative / 'Fixture.nsproj').write_text(project('Fixture', c_reference()), encoding='utf-8')
        output = run('O0-33-parametros', [args.nsc, '--dump-project-oop-llvm', 'Fixture.nsproj',
                     '--cpp-clang-root=' + args.clang_root], negative, expected=1)
        if 'preserve_none does not yet support this signature' not in output:
            raise SystemExit('firma C preserve_none de 33 parametros no rechazada como se esperaba; evidencia: ' + str(root))
        print('O0: firma C preserve_none de 33 parametros: Clang acepta y NSC rechaza', flush=True)
        # A variadic preserve_none function is rejected earlier, at import; that capability stays open
        # (t09-preserve-none-variadica-observacion.md). Its exact diagnostic is checked separately.
        negative = root / 'O0-variadica'; negative.mkdir()
        shutil.copyfile(case / 'api.lib', negative / 'api.lib')
        if args.dll: shutil.copyfile(case / 'api.dll', negative / 'api.dll')
        (negative / 'api.h').write_text((case / 'api.h').read_text(encoding='utf-8') +
            'API int PN t09_pn_variadic(int count, ...);\n', encoding='utf-8')
        (negative / 'variadic.c').write_text('#include "api.h"\nint PN t09_pn_variadic(int count, ...) '
            '{ return count + 1; }\n', encoding='utf-8')
        run('O0-variadica-clang', ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c17', '-O0',
            *(['-DT09_EXPORT'] if args.dll else []), '-c', 'variadic.c', '-o', 'variadic.obj'], negative)
        (negative / 'Main.ns').write_text('int Main() { return t09_pn_variadic(1, 2); }\n', encoding='utf-8')
        (negative / 'Fixture.nsproj').write_text(project('Fixture', c_reference()), encoding='utf-8')
        output = run('O0-variadica', [args.nsc, '--dump-project-oop-llvm', 'Fixture.nsproj',
                     '--cpp-clang-root=' + args.clang_root], negative, expected=1)
        if 'variadic function calling convention is not yet projected' not in output:
            raise SystemExit('variadica C preserve_none no rechazada como se esperaba; evidencia: ' + str(root))
        print('O0: variadica C preserve_none: Clang acepta y NSC la rechaza al importar', flush=True)
print('OK: firmas C preserve_none por plan; evidencia:', root)
