"""Compara ocho tamaños para punteros desplazados, en directo y precompilados."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
parser.add_argument('--sysv', action='store_true')
parser.add_argument('--c', action='store_true', help='Usa el fixture de biblioteca C')
parser.add_argument('--solo-efecto', action='store_true')
args = parser.parse_args()
source = (Path(__file__).resolve().parent.parent / 'pruebas-tanda-09-c-tamano-objeto' if args.c
          else Path(__file__).resolve().parent)
root = Path(tempfile.mkdtemp(prefix='nsc-t09-pointer-offset-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)

def run(label, command, cwd, expect_zero=True):
    result = subprocess.run(command, cwd=cwd, env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    (cwd / (label + '.log')).write_text('COMMAND: ' + repr(command) + '\nEXIT: '
        + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
    if expect_zero and result.returncode:
        raise SystemExit(label + ': fallo; evidencia: ' + str(cwd / (label + '.log')))
    return result.returncode

if 'clang version 23.1.0' not in subprocess.run(['clang.exe', '--version'], env=env,
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True).stdout:
    raise SystemExit('Se requiere Clang 23.1.0')

names = (('t09_s0', 't09_s1', 't09_s2', 't09_s3', 't09_d0', 't09_d1', 't09_d2', 't09_d3') if args.c else
         ('size0', 'size1', 'size2', 'size3', 'dynamic0', 'dynamic1', 'dynamic2', 'dynamic3'))
def measure(language, offset, expected_calls=0):
    cpp = language == 'cpp'
    prefix = '' if args.c else ('T09ObjectSize::' if cpp else 'T09ObjectSize.')
    lines = ['int result=0;', 'int value=0;']
    for index, name in enumerate(names):
        weight = 8 ** index
        lines.append('value=' + prefix + name + '(p+' + offset + ');')
        for i, value in enumerate((-1, 0, 4, 8, 12, 16)):
            lines.append(('if' if i == 0 else 'else if') + f' (value=={value}) result=result+{i * weight};')
        lines.append('else return 2147483647;')
    if expected_calls:
        counter = 't09_index_calls()' if args.c else prefix + 'indexCalls()'
        lines.append('if (' + counter + '!=' + str(expected_calls) + ') return 2147483646;')
    return '\n'.join(lines) + '\nreturn result;'

def body(language, case):
    cpp = language == 'cpp'
    prefix = '' if args.c else ('T09ObjectSize::' if cpp else 'T09ObjectSize.')
    index_function = 't09_index()' if args.c else prefix + 'index()'
    if case in ('pila', 'efecto-pila'):
        setup = ('int count=' + index_function + '+2; int* p=' +
                 ('(int*)__builtin_alloca(count*sizeof(int));' if cpp else 'stackalloc int[count];'))
    elif args.c:
        setup = ('struct T09CArray a; int* p=&a.values[0];' if cpp else 'T09CArray a; int* p=&a.values[0];')
    else:
        setup = ('T09ObjectSize::ArrayPair a; int* p=&a.values[0];' if cpp else
                 'T09ObjectSize.ArrayPair a; int* p=&a.values[0];')
    if case == 'parametro':
        return setup + '\nreturn Check(p);'
    if case == 'indice':
        setup += '\nint offset=' + index_function + ';'
    has_effect = case in ('efecto', 'efecto-pila')
    offset = 'offset' if case == 'indice' else '(' + index_function + '+0)' if has_effect else '1'
    return setup + '\n' + measure(language, offset, 9 if case == 'efecto-pila' else 8 if has_effect else 0)

for mode in ('estatica', 'dll'):
    for opt in range(4):
        for case in (('efecto', 'efecto-pila') if args.solo_efecto else ('pila', 'campo', 'parametro', 'indice', 'efecto', 'efecto-pila')):
            work = root / (mode + '-O' + str(opt) + '-' + case)
            work.mkdir()
            api_source = 'api.c' if args.c else 'api.cpp'
            for name in ('api.h', api_source):
                shutil.copyfile(source / name, work / name)
            header = work / 'api.h'
            content = header.read_text(encoding='utf-8').replace('int *p __attribute__', 'int *const p __attribute__').replace(
                'int *q __attribute__', 'int *const q __attribute__')
            if args.c:
                content += '\nT09_STORAGE int t09_index(void);\nT09_STORAGE int t09_index_calls(void);\n'
                path = work / api_source
                path.write_text(path.read_text(encoding='utf-8') + '\nstatic int indexCount;\n'
                    'int t09_index(void) { ++indexCount; return 1; }\nint t09_index_calls(void) { return indexCount; }\n', encoding='utf-8')
            else:
                content = content.replace('T09_STORAGE int index();', 'T09_STORAGE int index();\nT09_STORAGE int indexCalls();')
                path = work / api_source
                api = path.read_text(encoding='utf-8')
                old = 'int T09ObjectSize::index() { return 1; }'
                if api.count(old) != 1:
                    raise SystemExit('no se encontro el contador de indice')
                path.write_text(api.replace(old, 'namespace { int indexCount; }\n'
                    'int T09ObjectSize::index() { ++indexCount; return 1; }\n'
                    'int T09ObjectSize::indexCalls() { return indexCount; }'), encoding='utf-8')
            if args.sysv:
                content = content.replace('T09_STORAGE int ', 'T09_STORAGE __attribute__((sysv_abi)) int ')
            header.write_text(content, encoding='utf-8')
            flags = ['-target', 'x86_64-pc-windows-msvc', '-std=c11' if args.c else '-std=c++20', '-mno-stack-arg-probe', '-O' + str(opt)]
            run('api', ['clang.exe', *flags, *(['-DT09_EXPORT'] if mode == 'dll' else []), '-c', api_source, '-o', 'api.obj'], work)
            if mode == 'dll':
                run('dll', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib', '/out:api.dll', '/implib:api.lib', 'api.obj'], work)
                header.write_text('#define T09_IMPORT 1\n' + header.read_text(encoding='utf-8'), encoding='utf-8')
            else:
                run('lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], work)
            control_source = 'Control.c' if args.c else 'Control.cpp'
            cpp_helper = 'int Check(int* p) {\n' + measure('cpp', '1') + '\n}\n' if case == 'parametro' else ''
            (work / control_source).write_text('#include "api.h"\n' + cpp_helper + 'int main() {\n' + body('cpp', case) + '\n}\n', encoding='utf-8')
            run('control-obj', ['clang.exe', *flags, '-c', control_source, '-o', 'Control.obj'], work)
            run('control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console', '/nodefaultlib',
                 '/out:control.exe', 'Control.obj', 'api.lib'], work)
            expected = run('control-run', [str(work / 'control.exe')], work, False)
            if expected < 0 or expected >= 8 ** len(names):
                raise SystemExit('resultado Clang inesperado: ' + str(work))
            runtime = ' Runtime="api.dll"' if mode == 'dll' else ''
            language = 'C' if args.c else 'C++'
            standard = 'c11' if args.c else 'c++20'
            reference = '<CppLibraryReference Header="api.h" Language="' + language + '" Standard="' + standard + '" Library="api.lib"' + runtime + ' />'
            common = ['--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O' + str(opt)]
            ns_helper = 'int Check(int* p) {\n' + measure('ns', '1') + '\n}\n' if case == 'parametro' else ''
            (work / 'Main.ns').write_text(ns_helper + 'int Main() {\n' + body('ns', case) + '\n}\n', encoding='utf-8')
            (work / 'Fixture.nsproj').write_text('<Project><PropertyGroup><Name>Fixture</Name></PropertyGroup><ItemGroup>'
                '<Compile Include="Main.ns" />' + reference + '</ItemGroup></Project>', encoding='utf-8')
            run('native-build', [args.nsc, '--emit-project-oop-exe', 'Fixture.nsproj', '--library-reference-root', str(work),
                                 *common, '-o', str(work / 'native.exe')], work)
            actual = run('native-run', [str(work / 'native.exe')], work, False)
            if actual != expected:
                raise SystemExit('resultado directo distinto; evidencia: ' + str(work))
            producer_helper = 'public static int Check(int* p) {\n' + measure('ns', '1') + '\n}\n' if case == 'parametro' else ''
            (work / 'Producer.ns').write_text('namespace T09 { public class Bridge { ' + producer_helper + 'public static int Run<T>(T unused) {\n'
                                             + body('ns', case) + '\n} } }\n', encoding='utf-8')
            (work / 'Producer.nsproj').write_text('<Project><PropertyGroup><Name>T09Producer</Name><OutputType>Library</OutputType>'
                '</PropertyGroup><ItemGroup><Compile Include="Producer.ns" />' + reference + '</ItemGroup></Project>', encoding='utf-8')
            run('producer-build', [args.nsc, '--emit-project-oop-lib', 'Producer.nsproj', *common, '-o', str(work / 'Producer.lib')], work)
            for visibility in ('con', 'sin'):
                consumer = work / visibility
                consumer.mkdir()
                (consumer / 'Main.ns').write_text('int Main() { return T09.Bridge.Run<int>(0); }\n', encoding='utf-8')
                refs = '<LibraryReference Include="../Producer.lib" />'
                if visibility == 'con':
                    refs += '<CppLibraryReference Header="../api.h" Language="' + language + '" Standard="' + standard + '" Library="../api.lib"' + (
                        ' Runtime="../api.dll"' if mode == 'dll' else '') + ' />'
                else:
                    header.rename(work / 'api.h.oculto')
                (consumer / 'Consumer.nsproj').write_text('<Project><PropertyGroup><Name>Consumer</Name></PropertyGroup><ItemGroup>'
                    '<Compile Include="Main.ns" />' + refs + '</ItemGroup></Project>', encoding='utf-8')
                run('build', [args.nsc, '--emit-project-oop-exe', 'Consumer.nsproj', *common, '-o', str(consumer / 'consumer.exe')], consumer)
                if run('run', [str(consumer / 'consumer.exe')], consumer, False) != expected:
                    raise SystemExit('resultado precompilado distinto; evidencia: ' + str(consumer))
            print(mode, 'O' + str(opt), case, 'correcto', flush=True)
print('OK: punteros desplazados; evidencia:', root)
