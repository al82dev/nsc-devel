"""T09-003: records C por valor con regcall mezclados con escalares, sin expansion.

Compara un control C compilado por Clang 23.1.0 con un consumidor NativeSharp que llama
funciones C regcall cuyos records pasan directamente como un entero (8 y 4 bytes) o de
forma indirecta (36 bytes), junto a escalares, con retorno double, int, void, long long,
unsigned char, record compacto de 8 bytes y record indirecto de 36 bytes, y cuatro
punteros a funcion regcall con esas firmas. Biblioteca estatica y
DLL, O0-O3; con --precompilado tambien una plantilla, consumida con y sin encabezado, con
las mismas llamadas. Exige x86_regcallcc y el simbolo __regcall3__ en el IR de NSC,
tambien en llamadas indirectas y en el consumidor precompilado, y los valores devueltos.
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
root = Path(tempfile.mkdtemp(prefix='nsc-t09-regcall-c-record-mixto-', dir='A:/temp'))
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
#define RC __attribute__((regcall))
typedef struct T09Pair { int a; int b; } T09Pair;
typedef struct T09Small { short a; short b; } T09Small;
typedef struct T09Wide { int values[9]; } T09Wide;
API double RC t09_rc_mixed(float a, int *p, T09Pair r, double d);
API int RC t09_rc_two(T09Small s, T09Wide w, int k);
API void RC t09_rc_store(int *out, T09Pair r);
API long long RC t09_rc_sum(T09Wide w, long long k);
API unsigned char RC t09_rc_flag(T09Small s);
API T09Pair RC t09_rc_make(float a, T09Small s);
API T09Wide RC t09_rc_widen(T09Pair p, int k);
typedef double (RC *T09RcMixed)(float, int *, T09Pair, double);
typedef int (RC *T09RcTwo)(T09Small, T09Wide, int);
typedef T09Pair (RC *T09RcMake)(float, T09Small);
typedef T09Wide (RC *T09RcWiden)(T09Pair, int);
API T09RcMixed t09_rc_get_mixed(void);
API T09RcTwo t09_rc_get_two(void);
API T09RcMake t09_rc_get_make(void);
API T09RcWiden t09_rc_get_widen(void);
"""
SOURCE = """#include "api.h"
__declspec(selectany) int _fltused = 0;
double RC t09_rc_mixed(float a, int *p, T09Pair r, double d) { return a + *p + r.a * 3 + r.b + d; }
int RC t09_rc_two(T09Small s, T09Wide w, int k) { w.values[0] = 99; return s.a * 100 + s.b + w.values[8] * 7 + k; }
void RC t09_rc_store(int *out, T09Pair r) { *out = r.b * 100 + r.a; }
long long RC t09_rc_sum(T09Wide w, long long k)
{ long long total = k; for (int i = 0; i < 9; ++i) total += w.values[i] * (i + 1); return total; }
unsigned char RC t09_rc_flag(T09Small s) { return (unsigned char)(s.a + s.b * 2); }
T09Pair RC t09_rc_make(float a, T09Small s) { T09Pair r; r.a = s.a * 10 + (int)a; r.b = s.b; return r; }
T09Wide RC t09_rc_widen(T09Pair p, int k) { T09Wide w; for (int i = 0; i < 9; ++i) w.values[i] = p.a + i * p.b + k; return w; }
T09RcMixed t09_rc_get_mixed(void) { return t09_rc_mixed; }
T09RcTwo t09_rc_get_two(void) { return t09_rc_two; }
T09RcMake t09_rc_get_make(void) { return t09_rc_make; }
T09RcWiden t09_rc_get_widen(void) { return t09_rc_widen; }
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
    int value = 4; int stored = 0; T09Pair pair; T09Small small; T09Wide wide;
    pair.a = 10; pair.b = 20; small.a = 3; small.b = 5;
    for (int i = 0; i < 9; ++i) wide.values[i] = i + 1;
    if (t09_rc_mixed(0.5f, &value, pair, 0.25) != 54.75) return 1;
    if (t09_rc_two(small, wide, 2) != 370 || wide.values[0] != 1) return 2;
    t09_rc_store(&stored, pair); if (stored != 2010) return 3;
    if (t09_rc_sum(wide, 5000000000LL) != 5000000285LL) return 4;
    if (t09_rc_flag(small) != 13) return 5;
    { T09Pair made = t09_rc_make(2.5f, small); if (made.a != 32 || made.b != 5) return 8; }
    { T09Wide widened = t09_rc_widen(pair, 1); if (widened.values[0] != 11 || widened.values[8] != 171) return 9; }
    if (t09_rc_get_mixed()(0.5f, &value, pair, 0.25) != 54.75) return 6;
    if (t09_rc_get_two()(small, wide, 2) != 370) return 7;
    { T09Pair made = t09_rc_get_make()(2.5f, small); if (made.a != 32 || made.b != 5) return 10; }
    { T09Wide widened = t09_rc_get_widen()(pair, 1); if (widened.values[0] != 11 || widened.values[8] != 171) return 11; }
    return 0;
}
"""
BODY = ('int value = 4; int stored = 0; T09Pair pair; pair.a = 10; pair.b = 20; '
        'T09Small small; small.a = 3; small.b = 5; T09Wide wide; '
        'for (int i = 0; i < 9; i++) { wide.values[i] = i + 1; } '
        'if (t09_rc_mixed(0.5f, &value, pair, 0.25) != 54.75) return 11; '
        'if (t09_rc_two(small, wide, 2) != 370 || wide.values[0] != 1) return 12; '
        't09_rc_store(&stored, pair); if (stored != 2010) return 13; '
        'if (t09_rc_sum(wide, 5000000000L) != 5000000285L) return 14; '
        'if (t09_rc_flag(small) != 13) return 15; '
        'T09Pair made = t09_rc_make(2.5f, small); if (made.a != 32 || made.b != 5) return 18; '
        'T09Wide widened = t09_rc_widen(pair, 1); if (widened.values[0] != 11 || widened.values[8] != 171) return 19; ')
POINTER_BODY = ('T09RcMixed mixed = t09_rc_get_mixed(); '
        'if (mixed(0.5f, &value, pair, 0.25) != 54.75) return 16; '
        'T09RcTwo two = t09_rc_get_two(); '
        'if (two(small, wide, 2) != 370 || wide.values[0] != 1) return 17; '
        'T09RcMake maker = t09_rc_get_make(); T09Pair made2 = maker(2.5f, small); '
        'if (made2.a != 32 || made2.b != 5) return 20; '
        'T09RcWiden widener = t09_rc_get_widen(); T09Wide widened2 = widener(pair, 1); '
        'if (widened2.values[0] != 11 || widened2.values[8] != 171) return 21; ')
SYMBOLS = ('t09_rc_mixed', 't09_rc_two', 't09_rc_store', 't09_rc_sum', 't09_rc_flag', 't09_rc_make', 't09_rc_widen')


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
        if not re.search(r'define dso_local (?:dllexport )?x86_regcallcc [^@]*@__regcall3__' + symbol + r'\(', clang_ir):
            raise SystemExit(f'O{opt}: Clang no emite x86_regcallcc para {symbol}; evidencia: {root}')
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
        declaration = 'declare ' + ('dllimport ' if args.dll else '') + 'x86_regcallcc '
        if not re.search(r'call x86_regcallcc [^@]*@__regcall3__' + symbol + r'\(', ir) or \
                not any(line.startswith(declaration) and '@__regcall3__' + symbol + '(' in line for line in ir.splitlines()):
            raise SystemExit(f'O{opt}: convencion o simbolo NSC distinto para {symbol}; evidencia: {root}')
    for result in ('double', 'i32', 'i64', 'void'):
        if not re.search(r'call x86_regcallcc (?:noundef )?' + result + r' %[\w.]+\(', ir):
            raise SystemExit(f'O{opt}: llamada por puntero x86_regcallcc {result} ausente; evidencia: {root}')
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
            for result in ('double', 'i32', 'i64', 'void'):
                if not re.search(r'call x86_regcallcc (?:noundef )?' + result + r' %[\w.]+\(', consumer_ir):
                    raise SystemExit(f'O{opt} {visibility}: llamada por puntero x86_regcallcc {result} ausente; '
                                     f'evidencia: {root}')
            run(f'O{opt}-{visibility}-build', [args.nsc, '--emit-project-oop-exe', 'Consumer.nsproj',
                '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O' + str(opt), '-o',
                'consumer.exe'], consumer)
            if args.dll: shutil.copyfile(producer / 'api.dll', consumer / 'api.dll')
            run(f'O{opt}-{visibility}-run', [str(consumer / 'consumer.exe')], consumer)
            print(f'O{opt}: plantilla precompilada {visibility} paso', flush=True)
print('OK: records C regcall sin expansion, directos y por puntero; evidencia:', root)
