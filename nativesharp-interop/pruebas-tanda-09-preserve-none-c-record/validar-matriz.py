"""Contrasta record C preserve_none por valor, también precompilado."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
parser.add_argument('--dll', action='store_true')
parser.add_argument('--precompilado', action='store_true')
args = parser.parse_args()
root = Path(tempfile.mkdtemp(prefix='nsc-t09-preserve-c-record-matriz-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)


def run(label, command, cwd):
    result = subprocess.run(command, cwd=cwd, env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    (root / (label + '.log')).write_text('COMMAND: ' + repr(command) + '\nEXIT: '
        + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
    if result.returncode:
        print(result.stdout, flush=True)
        raise SystemExit(label + ': fallo; evidencia: ' + str(root))
    return result.stdout


def project(name, refs):
    return ('<Project><PropertyGroup><Name>' + name + '</Name></PropertyGroup><ItemGroup>'
        '<Compile Include="Main.ns" />' + refs + '</ItemGroup></Project>')


def reference(prefix=''):
    runtime = ' Runtime="' + prefix + 'api.dll"' if args.dll else ''
    return ('<CppLibraryReference Header="' + prefix + 'api.h" Language="C" Standard="c17" Library="'
        + prefix + 'api.lib"' + runtime + ' />')


HEADER = '''#pragma once
#if defined(T09_EXPORT)
#define API __declspec(dllexport)
#elif defined(T09_IMPORT)
#define API __declspec(dllimport)
#else
#define API
#endif
typedef struct T09Small { int x, y; int padding[7]; } T09Small;
API T09Small __attribute__((preserve_none)) t09_c_record(T09Small p);
'''
SOURCE = '''#include "api.h"
T09Small __attribute__((preserve_none)) t09_c_record(T09Small p)
{ p.x += 3; p.y -= 2; p.padding[6] += 4; return p; }
'''
CONTROL = '''#include "api.h"
void *memcpy(void *d, const void *s, unsigned long long n) {
    volatile unsigned char *o = d; const volatile unsigned char *i = s;
    for (unsigned long long k = 0; k < n; ++k) o[k] = i[k]; return d;
}
int main(void) { T09Small p; p.x=10; p.y=20; p.padding[0]=31; p.padding[1]=32;
    p.padding[2]=33; p.padding[3]=34; p.padding[4]=35; p.padding[5]=36; p.padding[6]=30;
    T09Small q=t09_c_record(p);
    return p.x==10 && p.y==20 && p.padding[6]==30 && q.x==13 && q.y==18 &&
        q.padding[0]==31 && q.padding[1]==32 && q.padding[2]==33 && q.padding[3]==34 &&
        q.padding[4]==35 && q.padding[5]==36 && q.padding[6]==34 ? 0 : 1; }
'''
BODY = ('T09Small p; p.x = 10; p.y = 20; p.padding[0] = 31; p.padding[1] = 32; '
    'p.padding[2] = 33; p.padding[3] = 34; p.padding[4] = 35; p.padding[5] = 36; p.padding[6] = 30; '
    'T09Small q = t09_c_record(p); '
    'return p.x == 10 && p.y == 20 && p.padding[6] == 30 && q.x == 13 && q.y == 18 && '
    'q.padding[0] == 31 && q.padding[1] == 32 && q.padding[2] == 33 && q.padding[3] == 34 && '
    'q.padding[4] == 35 && q.padding[5] == 36 && q.padding[6] == 34 ? 0 : 1;')

if 'clang version 23.1.0' not in run('version', ['clang.exe', '--version'], root):
    raise SystemExit('Se requiere Clang 23.1.0')
for opt in range(4):
    case = root / ('O' + str(opt)); case.mkdir()
    (case / 'api.h').write_text(HEADER, encoding='utf-8')
    (case / 'api.c').write_text(SOURCE, encoding='utf-8')
    (case / 'Control.c').write_text(CONTROL, encoding='utf-8')
    flags = ['-target', 'x86_64-pc-windows-msvc', '-std=c17', '-O' + str(opt)]
    run(f'O{opt}-api', ['clang.exe', *flags, *(['-DT09_EXPORT'] if args.dll else []), '-c', 'api.c', '-o', 'api.obj'], case)
    clang_ir = run(f'O{opt}-clang-ir', ['clang.exe', *flags, '-O0', '-S', '-emit-llvm', 'api.c', '-o', '-'], case)
    if not any('define ' in line and 'preserve_nonecc void @t09_c_record(' in line and 'sret(' in line
               for line in clang_ir.splitlines()):
        raise SystemExit(f'O{opt}: firma Clang inesperada: {root}')
    if args.dll:
        run(f'O{opt}-lib', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib',
            '/out:api.dll', '/implib:api.lib', 'api.obj'], case)
        (case / 'api.h').write_text('#define T09_IMPORT 1\n' + HEADER, encoding='utf-8')
    else:
        run(f'O{opt}-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], case)
    run(f'O{opt}-control-obj', ['clang.exe', *flags, '-c', 'Control.c', '-o', 'control.obj'], case)
    run(f'O{opt}-control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console', '/nodefaultlib',
        '/out:control.exe', 'control.obj', 'api.lib'], case)
    run(f'O{opt}-control-run', [str(case / 'control.exe')], case)
    (case / 'Main.ns').write_text('int Main() { ' + BODY + ' }\n', encoding='utf-8')
    (case / 'Fixture.nsproj').write_text(project('Fixture', reference()), encoding='utf-8')
    common = ['Fixture.nsproj', '--cpp-clang-root=' + args.clang_root]
    ir = run(f'O{opt}-ir', [args.nsc, '--dump-project-oop-llvm', *common], case)
    if not any('call preserve_nonecc void @t09_c_record(' in line and 'sret([36 x i8]) align 4' in line
               for line in ir.splitlines()):
        raise SystemExit(f'O{opt}: llamada NSC inesperada: {root}')
    run(f'O{opt}-nsc', [args.nsc, '--emit-project-oop-exe', *common, '--no-artifact-cache',
        '-O' + str(opt), '-o', 'native.exe'], case)
    run(f'O{opt}-native', [str(case / 'native.exe')], case)
    if args.precompilado:
        producer = case / 'producer'; producer.mkdir()
        for name in ('api.h', 'api.lib') + (('api.dll',) if args.dll else ()):
            shutil.copyfile(case / name, producer / name)
        (producer / 'Producer.ns').write_text('namespace T09 { public class Bridge { '
            'public static int Run<T>(T unused) { ' + BODY + ' } } }\n', encoding='utf-8')
        (producer / 'Producer.nsproj').write_text('<Project><PropertyGroup><Name>T09Producer</Name>'
            '<OutputType>Library</OutputType></PropertyGroup><ItemGroup><Compile Include="Producer.ns" />'
            + reference() + '</ItemGroup></Project>', encoding='utf-8')
        run(f'O{opt}-producer', [args.nsc, '--emit-project-oop-lib', 'Producer.nsproj',
            '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O' + str(opt), '-o', 'Producer.lib'], producer)
        for visibility in ('con-encabezado', 'sin-encabezado'):
            consumer = case / visibility; consumer.mkdir()
            refs = '<LibraryReference Include="../producer/Producer.lib" />'
            if visibility == 'con-encabezado': refs += reference('../producer/')
            else: (producer / 'api.h').rename(producer / 'api.h.oculto')
            (consumer / 'Main.ns').write_text('int Main() { return T09.Bridge.Run<int>(0); }\n', encoding='utf-8')
            (consumer / 'Consumer.nsproj').write_text(project('T09Consumer', refs), encoding='utf-8')
            run(f'O{opt}-{visibility}-build', [args.nsc, '--emit-project-oop-exe', 'Consumer.nsproj',
                '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O' + str(opt), '-o', 'consumer.exe'], consumer)
            if args.dll: shutil.copyfile(producer / 'api.dll', consumer / 'api.dll')
            run(f'O{opt}-{visibility}-run', [str(consumer / 'consumer.exe')], consumer)
    print(f'O{opt}: controles C, NSC y precompilados solicitados pasaron', flush=True)
print('OK: record C preserve_none; evidencia:', root)
