"""T09-003: records C por valor con preserve_none admitidos por el plan ABI de Clang.

Compara un control C compilado por Clang 23.1.0 con un consumidor NativeSharp que llama
funciones C preserve_none con records mezclados con escalares, records distintos en
parametro y retorno, dos records en una firma, y punteros a funcion con record de
36 bytes de ida y vuelta y con firma mixta. Biblioteca estatica y DLL, O0-O3; con
--precompilado tambien una plantilla, consumida con y sin encabezado, con las mismas
llamadas directas y por puntero. Exige preserve_nonecc en declaraciones y llamadas
indirectas del IR de NSC, tambien en el consumidor precompilado, y los valores de cada campo.
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
root = Path(tempfile.mkdtemp(prefix='nsc-t09-preserve-none-c-record-mixto-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)

HEADER = """#pragma once
#if defined(T09_EXPORT)
#define API __declspec(dllexport)
#elif defined(T09_IMPORT)
#define API __declspec(dllimport)
#else
#define API
#endif
#define PN __attribute__((preserve_none))
typedef struct T09Pair { int a; int b; } T09Pair;
typedef struct T09In { int values[9]; } T09In;
typedef struct T09Out { int values[9]; } T09Out;
API double PN t09_pn_record_mixed(float a, int *p, T09Pair r, double d);
API T09Out PN t09_pn_convert(T09In value);
API int PN t09_pn_two(T09In first, T09Pair second, int k);
typedef T09In (PN *T09PnWide)(T09In);
typedef double (PN *T09PnRecordMixed)(float, int *, T09Pair, double);
API T09In PN t09_pn_wide(T09In value);
API T09PnWide t09_pn_get_wide(void);
API T09PnRecordMixed t09_pn_get_record_mixed(void);
"""
SOURCE = """#include "api.h"
__declspec(selectany) int _fltused = 0;
double PN t09_pn_record_mixed(float a, int *p, T09Pair r, double d) { return a + *p + r.a + r.b + d; }
T09Out PN t09_pn_convert(T09In value)
{ T09Out result; for (int i = 0; i < 9; ++i) result.values[i] = value.values[i] * 2; return result; }
int PN t09_pn_two(T09In first, T09Pair second, int k) { return first.values[8] + second.a * second.b + k; }
T09In PN t09_pn_wide(T09In value) { value.values[0] += 1; value.values[8] += 2; return value; }
T09PnWide t09_pn_get_wide(void) { return t09_pn_wide; }
T09PnRecordMixed t09_pn_get_record_mixed(void) { return t09_pn_record_mixed; }
"""
CONTROL = """#include "api.h"
__declspec(selectany) int _fltused = 0;
void *memcpy(void *d, const void *s, unsigned long long n)
{
    volatile unsigned char *o = d; const volatile unsigned char *i = s;
    for (unsigned long long k = 0; k < n; ++k) o[k] = i[k];
    return d;
}
int main(void)
{
    int value = 4; T09Pair pair; T09In in; T09Out out; T09In wide;
    pair.a = 10; pair.b = 20;
    for (int i = 0; i < 9; ++i) in.values[i] = i + 1;
    if (t09_pn_record_mixed(0.5f, &value, pair, 0.25) != 34.75) return 1;
    out = t09_pn_convert(in); if (out.values[0] != 2 || out.values[8] != 18 || in.values[8] != 9) return 2;
    if (t09_pn_two(in, pair, 3) != 212) return 3;
    wide = t09_pn_get_wide()(in); if (wide.values[0] != 2 || wide.values[8] != 11) return 4;
    if (t09_pn_get_record_mixed()(0.5f, &value, pair, 0.25) != 34.75) return 5;
    return 0;
}
"""
BODY = ('int value = 4; T09Pair pair; pair.a = 10; pair.b = 20; T09In input; '
        'for (int i = 0; i < 9; i++) { input.values[i] = i + 1; } '
        'if (t09_pn_record_mixed(0.5f, &value, pair, 0.25) != 34.75) return 11; '
        'T09Out output = t09_pn_convert(input); '
        'if (output.values[0] != 2 || output.values[8] != 18 || input.values[8] != 9) return 12; '
        'if (t09_pn_two(input, pair, 3) != 212) return 13; ')
POINTER_BODY = ('T09PnWide wide = t09_pn_get_wide(); T09In widened = wide(input); '
        'if (widened.values[0] != 2 || widened.values[8] != 11 || input.values[0] != 1) return 14; '
        'T09PnRecordMixed mixed = t09_pn_get_record_mixed(); '
        'if (mixed(0.5f, &value, pair, 0.25) != 34.75) return 15; ')
SYMBOLS = ('t09_pn_record_mixed', 't09_pn_convert', 't09_pn_two')


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
    (case / 'Main.ns').write_text('int Main() { ' + BODY + POINTER_BODY + 'return 0; }\n', encoding='utf-8')
    (case / 'Fixture.nsproj').write_text(project('Fixture', c_reference()), encoding='utf-8')
    common = ['Fixture.nsproj', '--cpp-clang-root=' + args.clang_root]
    ir = run(f'O{opt}-ir', [args.nsc, '--dump-project-oop-llvm', *common], case)
    for symbol in SYMBOLS:
        declaration = 'declare ' + ('dllimport ' if args.dll else '') + 'preserve_nonecc '
        if not re.search(r'call preserve_nonecc [^@]*@' + symbol + r'\(', ir) or \
                not any(line.startswith(declaration) and '@' + symbol + '(' in line for line in ir.splitlines()):
            raise SystemExit(f'O{opt}: convencion o simbolo NSC distinto para {symbol}; evidencia: {root}')
    for result in ('void', 'double'):
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
            + BODY + POINTER_BODY + 'return 0; } } }\n', encoding='utf-8')
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
            consumer_ir = run(f'O{opt}-{visibility}-ir', [args.nsc, '--dump-project-oop-llvm', 'Consumer.nsproj',
                '--cpp-clang-root=' + args.clang_root], consumer)
            for result in ('void', 'double'):
                if not re.search(r'call preserve_nonecc (?:noundef )?' + result + r' %[\w.]+\(', consumer_ir):
                    raise SystemExit(f'O{opt} {visibility}: llamada por puntero preserve_nonecc {result} ausente; '
                                     f'evidencia: {root}')
            run(f'O{opt}-{visibility}-build', [args.nsc, '--emit-project-oop-exe', 'Consumer.nsproj',
                '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O' + str(opt), '-o',
                'consumer.exe'], consumer)
            if args.dll: shutil.copyfile(producer / 'api.dll', consumer / 'api.dll')
            run(f'O{opt}-{visibility}-run', [str(consumer / 'consumer.exe')], consumer)
            print(f'O{opt}: plantilla precompilada {visibility} paso', flush=True)
print('OK: records C preserve_none mixtos, distintos y por puntero; evidencia:', root)
