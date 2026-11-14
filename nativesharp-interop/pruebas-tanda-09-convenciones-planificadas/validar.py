"""T09-003: swiftcall, swiftasynccall e intel_ocl_bicc con firmas escalares y de punteros.

Para cada convencion compila con Clang 23.1.0 funciones C (y C++ en namespace para las dos
convenciones Swift, porque Clang no decora intel_ocl_bicc en C++) con firmas escalares y de
punteros, y un control C. El consumidor NativeSharp llama directamente esas funciones en
biblioteca estatica o DLL, O0-O3; con --precompilado tambien desde una plantilla consumida con
y sin encabezado. Exige en el IR de NSC la convencion LLVM (swiftcc, swifttailcc,
intel_ocl_bicc) y el simbolo de la definicion de Clang en cada declaracion y llamada, y los
valores devueltos. Las llamadas por puntero no forman parte de esta prueba: NativeSharp no
tiene tipos de funcion con estas convenciones.
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
root = Path(tempfile.mkdtemp(prefix='nsc-t09-convenciones-planificadas-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)

CASES = (('C', 'swiftcall', 'swiftcc'), ('C', 'swiftasynccall', 'swifttailcc'), ('C', 'intel_ocl_bicc', 'intel_ocl_bicc'),
         ('C++', 'swiftcall', 'swiftcc'), ('C++', 'swiftasynccall', 'swifttailcc'))
NAMES = ('t09_add', 't09_mix', 't09_store', 't09_same', 't09_wide')


def header(language, convention):
    ns_open, ns_close = ('namespace T09Pc {\n', '}\n') if language == 'C++' else ('', '')
    return ('#pragma once\n#if defined(T09_EXPORT)\n#define API __declspec(dllexport)\n#elif defined(T09_IMPORT)\n'
            '#define API __declspec(dllimport)\n#else\n#define API\n#endif\n'
            f'#define CC __attribute__(({convention}))\n' + ns_open +
            'API int CC t09_add(int a, int b);\n'
            'API double CC t09_mix(float a, double b, int *p);\n'
            'API void CC t09_store(int *out, long long v);\n'
            'API int *CC t09_same(int *p);\n'
            'API unsigned long long CC t09_wide(unsigned int a, long long b);\n' + ns_close)


def source(language):
    ns_open, ns_close = ('namespace T09Pc {\n', '}\n') if language == 'C++' else ('', '')
    return ('#include "api.h"\nextern "C" { __declspec(selectany) int _fltused = 0; }\n' if language == 'C++' else
            '#include "api.h"\n__declspec(selectany) int _fltused = 0;\n') + ns_open + (
        'int CC t09_add(int a, int b) { return a * 10 + b; }\n'
        'double CC t09_mix(float a, double b, int *p) { return a + b + *p; }\n'
        'void CC t09_store(int *out, long long v) { *out = (int)(v / 1000); }\n'
        'int *CC t09_same(int *p) { return p; }\n'
        'unsigned long long CC t09_wide(unsigned int a, long long b) { return a + (unsigned long long)b * 2; }\n') + ns_close


def control(language):
    scope = 'T09Pc::' if language == 'C++' else ''
    entry = 'extern "C" int main()' if language == 'C++' else 'int main(void)'
    fltused = ('extern "C" { __declspec(selectany) int _fltused = 0; }\n' if language == 'C++' else
               '__declspec(selectany) int _fltused = 0;\n')
    return ('#include "api.h"\n' + fltused + entry + '\n{\n'
            '    int value = 4, out = 0;\n'
            f'    if ({scope}t09_add(2, 3) != 23) return 1;\n'
            f'    if ({scope}t09_mix(0.5f, 0.25, &value) != 4.75) return 2;\n'
            f'    {scope}t09_store(&out, 5000000000LL); if (out != 5000000) return 3;\n'
            f'    if (*{scope}t09_same(&value) != 4) return 4;\n'
            f'    if ({scope}t09_wide(4000000000u, 3000000000LL) != 10000000000ULL) return 5;\n'
            '    return 0;\n}\n')


def body(language):
    scope = 'T09Pc.' if language == 'C++' else ''
    return ('int value = 4; int out = 0; '
            f'if ({scope}t09_add(2, 3) != 23) return 11; '
            f'if ({scope}t09_mix(0.5f, 0.25, &value) != 4.75) return 12; '
            f'{scope}t09_store(&out, 5000000000L); if (out != 5000000) return 13; '
            f'if (*{scope}t09_same(&value) != 4) return 14; '
            f'if ({scope}t09_wide((uint)4000000000, 3000000000L) != (ulong)10000000000) return 15; ')


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


def project(name, references, compile_name='Main.ns'):
    return ('<Project><PropertyGroup><Name>' + name + '</Name></PropertyGroup><ItemGroup>'
            '<Compile Include="' + compile_name + '" />' + references + '</ItemGroup></Project>')


def cpp_reference(language, prefix=''):
    runtime = ' Runtime="' + prefix + 'api.dll"' if args.dll else ''
    standard = 'c++20' if language == 'C++' else 'c17'
    return ('<CppLibraryReference Header="' + prefix + 'api.h" Language="' + language + '" Standard="' + standard
            + '" Library="' + prefix + 'api.lib"' + runtime + ' />')


def check_ir(label, ir, expected_cc, symbols):
    for symbol in symbols:
        quoted = '@"' + symbol + '"(' if not re.fullmatch(r'[\w.$]+', symbol) else '@' + symbol + '('
        declaration = 'declare ' + ('dllimport ' if args.dll else '') + expected_cc + ' '
        if not any(line.startswith(declaration) and quoted in line for line in ir.splitlines()):
            raise SystemExit(f'{label}: declaracion {expected_cc} de {symbol} ausente; evidencia: {root}')
        if not re.search(r'call ' + re.escape(expected_cc) + r' [^\n]*' + re.escape(quoted), ir):
            raise SystemExit(f'{label}: llamada {expected_cc} a {symbol} ausente; evidencia: {root}')


if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version'], root):
    raise SystemExit('Se requiere Clang 23.1.0')
for language, convention, expected_cc in CASES:
    extension = 'cpp' if language == 'C++' else 'c'
    standard = ['-std=c++20'] if language == 'C++' else ['-std=c17']
    for opt in range(4):
        tag = f'{language.replace("+", "p")}-{convention}-O{opt}'
        case = root / tag; case.mkdir()
        (case / 'api.h').write_text(header(language, convention), encoding='utf-8')
        (case / ('api.' + extension)).write_text(source(language), encoding='utf-8')
        (case / ('Control.' + extension)).write_text(control(language), encoding='utf-8')
        flags = ['-target', 'x86_64-pc-windows-msvc', *standard, '-O' + str(opt)]
        export = ['-DT09_EXPORT'] if args.dll else []
        run(f'{tag}-api', ['clang.exe', *flags, *export, '-c', 'api.' + extension, '-o', 'api.obj'], case)
        clang_ir = run(f'{tag}-api-ir', ['clang.exe', *flags, *export, '-O0', '-S', '-emit-llvm', 'api.' + extension,
                                         '-o', '-'], case)
        symbols = []
        for name in NAMES:
            match = re.search(r'^define dso_local (?:dllexport )?' + re.escape(expected_cc) +
                              r' [^@\n]*@("?[^"(]*' + name + r'[^"(]*"?)\(', clang_ir, re.M)
            if not match:
                raise SystemExit(f'{tag}: Clang no emite {expected_cc} para {name}; evidencia: {root}')
            symbols.append(match.group(1).strip('"'))
        if args.dll:
            run(f'{tag}-lib', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib', '/out:api.dll',
                                '/implib:api.lib', 'api.obj'], case)
            path = case / 'api.h'; path.write_text('#define T09_IMPORT 1\n' + path.read_text(encoding='utf-8'),
                                                   encoding='utf-8')
        else:
            run(f'{tag}-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], case)
        run(f'{tag}-control-obj', ['clang.exe', *flags, '-c', 'Control.' + extension, '-o', 'control.obj'], case)
        run(f'{tag}-control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console',
                                     '/nodefaultlib', '/out:control.exe', 'control.obj', 'api.lib'], case)
        run(f'{tag}-control-run', [str(case / 'control.exe')], case)
        (case / 'Main.ns').write_text('int Main() { ' + body(language) + 'return 0; }\n', encoding='utf-8')
        (case / 'Fixture.nsproj').write_text(project('Fixture', cpp_reference(language)), encoding='utf-8')
        common = ['Fixture.nsproj', '--cpp-clang-root=' + args.clang_root]
        check_ir(tag, run(f'{tag}-ir', [args.nsc, '--dump-project-oop-llvm', *common], case), expected_cc, symbols)
        run(f'{tag}-build', [args.nsc, '--emit-project-oop-exe', *common, '--no-artifact-cache', '-O' + str(opt),
                              '-o', 'native.exe'], case)
        run(f'{tag}-native-run', [str(case / 'native.exe')], case)
        print(f'{tag}: control y NativeSharp directo pasaron', flush=True)
        if not args.precompilado: continue
        producer = case / 'producer'; producer.mkdir()
        for name in ('api.h', 'api.lib') + (('api.dll',) if args.dll else ()):
            shutil.copyfile(case / name, producer / name)
        (producer / 'Producer.ns').write_text('namespace T09 { public class Bridge { public static int Run<T>(T unused) { '
            + body(language) + 'return 0; } } }\n', encoding='utf-8')
        (producer / 'Producer.nsproj').write_text('<Project><PropertyGroup><Name>T09Producer</Name>'
            '<OutputType>Library</OutputType></PropertyGroup><ItemGroup><Compile Include="Producer.ns" />'
            + cpp_reference(language) + '</ItemGroup></Project>', encoding='utf-8')
        run(f'{tag}-producer', [args.nsc, '--emit-project-oop-lib', 'Producer.nsproj', '--cpp-clang-root='
            + args.clang_root, '--no-artifact-cache', '-O' + str(opt), '-o', 'Producer.lib'], producer)
        for visibility in ('con-encabezado', 'sin-encabezado'):
            consumer = case / visibility; consumer.mkdir()
            references = '<LibraryReference Include="../producer/Producer.lib" />'
            if visibility == 'con-encabezado':
                references += cpp_reference(language, '../producer/')
            else:
                (producer / 'api.h').rename(producer / 'api.h.oculto')
            (consumer / 'Main.ns').write_text('int Main() { return T09.Bridge.Run<int>(0); }\n', encoding='utf-8')
            (consumer / 'Consumer.nsproj').write_text(project('T09Consumer', references), encoding='utf-8')
            check_ir(f'{tag}-{visibility}', run(f'{tag}-{visibility}-ir', [args.nsc, '--dump-project-oop-llvm',
                     'Consumer.nsproj', '--cpp-clang-root=' + args.clang_root], consumer), expected_cc, symbols)
            run(f'{tag}-{visibility}-build', [args.nsc, '--emit-project-oop-exe', 'Consumer.nsproj',
                '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O' + str(opt), '-o',
                'consumer.exe'], consumer)
            if args.dll: shutil.copyfile(producer / 'api.dll', consumer / 'api.dll')
            run(f'{tag}-{visibility}-run', [str(consumer / 'consumer.exe')], consumer)
            print(f'{tag}: plantilla precompilada {visibility} paso', flush=True)
# Records by value keep the existing rejection: the shared validator covers scalars and pointers only.
negative = root / 'swiftcall-record'; negative.mkdir()
(negative / 'api.h').write_text('#pragma once\ntypedef struct T09Pair { int a; int b; } T09Pair;\n'
    'int __attribute__((swiftcall)) t09_pair(T09Pair p);\n', encoding='utf-8')
(negative / 'api.c').write_text('#include "api.h"\nint __attribute__((swiftcall)) t09_pair(T09Pair p) { return p.a + p.b; }\n',
    encoding='utf-8')
run('negativo-clang', ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c17', '-c', 'api.c', '-o', 'api.obj'], negative)
run('negativo-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], negative)
(negative / 'Main.ns').write_text('int Main() { T09Pair p; p.a = 1; p.b = 2; return t09_pair(p) == 3 ? 0 : 1; }\n', encoding='utf-8')
(negative / 'Fixture.nsproj').write_text(project('Fixture', '<CppLibraryReference Header="api.h" Language="C" '
    'Standard="c17" Library="api.lib" />'), encoding='utf-8')
output = run('negativo-nsc', [args.nsc, '--dump-project-oop-llvm', 'Fixture.nsproj', '--cpp-clang-root=' + args.clang_root],
             negative, expected=1)
if "C/C++ calling convention 'swiftcall' is not yet supported in HIR" not in output:
    raise SystemExit('swiftcall con record por valor no fue rechazado como se esperaba; evidencia: ' + str(root))
print('swiftcall con record por valor: Clang acepta y NSC rechaza', flush=True)
print('OK: convenciones planificadas swiftcall, swiftasynccall e intel_ocl_bicc; evidencia:', root)
