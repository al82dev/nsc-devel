"""T09-003: punteros a funcion C sysv_abi con records C por valor, segun el plan de Clang.

Compara un control C compilado por Clang 23.1.0 con un consumidor NativeSharp que llama
por punteros a funcion sysv_abi devueltos por C: records de 8 bytes enteros (i64), 12
bytes ({i64, i32}), 36 bytes (sret y byval), 8 bytes flotantes (<2 x float>), 16 bytes
mixtos ({double, i32}), y una firma mixta con escalares, puntero y record. Biblioteca
estatica y DLL, O0-O3; con --precompilado tambien una plantilla, consumida con y sin
encabezado, con las mismas llamadas. Exige x86_64_sysvcc en las llamadas indirectas del
IR de NSC, tambien en el consumidor precompilado, y los valores de cada campo.
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
root = Path(tempfile.mkdtemp(prefix='nsc-t09-sysv-c-puntero-records-', dir='A:/temp'))
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
#define SV __attribute__((sysv_abi))
typedef struct R8 { int a; int b; } R8;
typedef struct R12 { int a; int b; int c; } R12;
typedef struct R36 { int values[9]; } R36;
typedef struct RF { float x; float y; } RF;
typedef struct RM { double d; int i; } RM;
typedef R8 (SV *T09P8)(R8);
typedef R12 (SV *T09P12)(int, R12);
typedef R36 (SV *T09P36)(R36);
typedef RF (SV *T09PF)(RF);
typedef RM (SV *T09PM)(RM);
typedef double (SV *T09PMix)(float, int *, R8, double);
typedef void (SV *T09PVoid)(int *, R36, RF);
API T09P8 t09_get8(void);
API T09P12 t09_get12(void);
API T09P36 t09_get36(void);
API T09PF t09_getf(void);
API T09PM t09_getm(void);
API T09PMix t09_getmix(void);
API T09PVoid t09_getvoid(void);
"""
SOURCE = """#include "api.h"
__declspec(selectany) int _fltused = 0;
static R8 SV f8(R8 v) { R8 r; r.a = v.a * 2; r.b = v.b * 3; return r; }
static R12 SV f12(int k, R12 v) { R12 r; r.a = v.a + k; r.b = v.b - k; r.c = v.c * k; return r; }
static R36 SV f36(R36 v) { R36 r; for (int i = 0; i < 9; ++i) r.values[i] = v.values[i] + i * 10; return r; }
static RF SV ff(RF v) { RF r; r.x = v.y * 2.0f; r.y = v.x + 0.5f; return r; }
static RM SV fm(RM v) { RM r; r.d = v.d * 4.0; r.i = v.i - 7; return r; }
static double SV fmix(float a, int *p, R8 r, double d) { return a + *p + r.a * 3 + r.b + d; }
static void SV fvoid(int *out, R36 w, RF f) { *out = w.values[8] * 100 + (int)(f.x * 10.0f); }
T09P8 t09_get8(void) { return f8; }
T09P12 t09_get12(void) { return f12; }
T09P36 t09_get36(void) { return f36; }
T09PF t09_getf(void) { return ff; }
T09PM t09_getm(void) { return fm; }
T09PMix t09_getmix(void) { return fmix; }
T09PVoid t09_getvoid(void) { return fvoid; }
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
    int value = 4, out = 0; R8 r8; R12 r12; R36 r36; RF rf; RM rm;
    r8.a = 10; r8.b = 20; r12.a = 1; r12.b = 2; r12.c = 3; rf.x = 1.5f; rf.y = 2.25f; rm.d = 1.25; rm.i = 50;
    for (int i = 0; i < 9; ++i) r36.values[i] = i + 1;
    { R8 r = t09_get8()(r8); if (r.a != 20 || r.b != 60 || r8.a != 10) return 1; }
    { R12 r = t09_get12()(5, r12); if (r.a != 6 || r.b != -3 || r.c != 15 || r12.a != 1) return 2; }
    { R36 r = t09_get36()(r36); if (r.values[0] != 1 || r.values[8] != 89 || r36.values[8] != 9) return 3; }
    { RF r = t09_getf()(rf); if (r.x != 4.5f || r.y != 2.0f || rf.x != 1.5f) return 4; }
    { RM r = t09_getm()(rm); if (r.d != 5.0 || r.i != 43 || rm.i != 50) return 5; }
    if (t09_getmix()(0.5f, &value, r8, 0.25) != 54.75) return 6;
    t09_getvoid()(&out, r36, rf); if (out != 915) return 7;
    return 0;
}
"""
BODY = ('int value = 4; int out = 0; R8 r8; r8.a = 10; r8.b = 20; R12 r12; r12.a = 1; r12.b = 2; r12.c = 3; '
        'RF rf; rf.x = 1.5f; rf.y = 2.25f; RM rm; rm.d = 1.25; rm.i = 50; R36 r36; '
        'for (int i = 0; i < 9; i++) { r36.values[i] = i + 1; } '
        'T09P8 p8 = t09_get8(); R8 q8 = p8(r8); if (q8.a != 20 || q8.b != 60 || r8.a != 10) return 11; '
        'T09P12 p12 = t09_get12(); R12 q12 = p12(5, r12); '
        'if (q12.a != 6 || q12.b != -3 || q12.c != 15 || r12.a != 1) return 12; '
        'T09P36 p36 = t09_get36(); R36 q36 = p36(r36); '
        'if (q36.values[0] != 1 || q36.values[8] != 89 || r36.values[8] != 9) return 13; '
        'T09PF pf = t09_getf(); RF qf = pf(rf); if (qf.x != 4.5f || qf.y != 2.0f || rf.x != 1.5f) return 14; '
        'T09PM pm = t09_getm(); RM qm = pm(rm); if (qm.d != 5.0 || qm.i != 43 || rm.i != 50) return 15; '
        'T09PMix pmix = t09_getmix(); if (pmix(0.5f, &value, r8, 0.25) != 54.75) return 16; '
        'T09PVoid pvoid = t09_getvoid(); pvoid(&out, r36, rf); if (out != 915) return 17; ')
POINTER_BODY = ''
SYMBOLS = ()
INDIRECT = ('i64', '{ i64, i32 }', 'void', '<2 x float>', '{ double, i32 }', 'double')


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
    if len(re.findall(r'define internal x86_64_sysvcc ', clang_ir)) != 7:
        raise SystemExit(f'O{opt}: Clang no emite las siete funciones x86_64_sysvcc; evidencia: {root}')
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
    for result in INDIRECT:
        if not re.search(r'call x86_64_sysvcc (?:noundef )?' + re.escape(result) + r' %[\w.]+\(', ir):
            raise SystemExit(f'O{opt}: llamada por puntero x86_64_sysvcc {result} ausente; evidencia: {root}')
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
            for result in INDIRECT:
                if not re.search(r'call x86_64_sysvcc (?:noundef )?' + re.escape(result) + r' %[\w.]+\(', consumer_ir):
                    raise SystemExit(f'O{opt} {visibility}: llamada por puntero x86_64_sysvcc {result} ausente; '
                                     f'evidencia: {root}')
            run(f'O{opt}-{visibility}-build', [args.nsc, '--emit-project-oop-exe', 'Consumer.nsproj',
                '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O' + str(opt), '-o',
                'consumer.exe'], consumer)
            if args.dll: shutil.copyfile(producer / 'api.dll', consumer / 'api.dll')
            run(f'O{opt}-{visibility}-run', [str(consumer / 'consumer.exe')], consumer)
            print(f'O{opt}: plantilla precompilada {visibility} paso', flush=True)
print('OK: punteros C sysv_abi con records por plan; evidencia:', root)
