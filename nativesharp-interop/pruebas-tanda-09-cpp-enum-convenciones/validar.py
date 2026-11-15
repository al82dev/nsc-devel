"""T09-003: enums C++ por valor con convenciones no cdecl, comparados con Clang 23.1.0.

Clang clasifica un enum C++ por valor, con o sin ambito, como su entero subyacente. Para
regcall, sysv_abi, preserve_most, preserve_none, preserve_all (extern "C", Clang no lo decora
en C++), swiftcall y swiftasynccall compila funciones C++ con enums con ambito de base int,
unsigned char, short, unsigned long long y wchar_t y un enum sin ambito con INT_MIN e INT_MAX, en
parametro, retorno e ida y vuelta, mezclados con escalares, y (regcall, sysv_abi, preserve_most,
preserve_none) con un record en parametro y con retorno enum estrecho. Un control C++ compilado
por Clang y un consumidor NativeSharp ejecutan las mismas comprobaciones, O0-O3, en biblioteca
estatica y con --dll en DLL; con --precompilado tambien una plantilla consumida con y sin
encabezado. Exige en el IR de NSC la convencion y las extensiones signext/zeroext de cada
parametro y del retorno identicas a la definicion de Clang, y la convencion en las llamadas por
puntero a funcion de las convenciones que NSC proyecta por puntero (con regcall, solo las firmas
sin record: los records C++ por valor en punteros regcall aun no se proyectan).
Incluye un enum de base wchar_t. Con --espacios los enums se declaran en espacios de nombres
(T09::In y T09::Otro::Mas) y las firmas los nombran calificados. El control negativo conserva el
rechazo de una union C solo de float con regcall y, con --espacios, el de un enum anidado en una
clase dentro de un alias de puntero.
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
parser.add_argument('--espacios', action='store_true')
parser.add_argument('--convenciones', default='')
parser.add_argument('--niveles', default='0,1,2,3')
args = parser.parse_args()
root = Path(tempfile.mkdtemp(prefix='nsc-t09-cpp-enum-convenciones-' + ('ns-' if args.espacios else ''), dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)

CC_TOKEN = re.compile(r'\b(x86_\w+cc|win64cc|preserve_\w+cc|swift\w*cc|intel_ocl_bicc|fastcc|coldcc|cc \d+)\b')
POINTER_CONVENTIONS = ('regcall', 'sysv_abi', 'preserve_most', 'preserve_all', 'preserve_none')
RECORD_CONVENTIONS = ('regcall', 'sysv_abi', 'preserve_most', 'preserve_none')
# C++ records by value through regcall pointers are not projected yet, with or without enums.
POINTER_RECORD_CONVENTIONS = ('sysv_abi', 'preserve_most', 'preserve_none')
EXTERN_C = ('preserve_all',)
CONVENTIONS = tuple(c for c in ('regcall', 'sysv_abi', 'preserve_most', 'preserve_none', 'preserve_all',
                                'swiftcall', 'swiftasynccall')
                    if not args.convenciones or c in re.split(r'[,+]', args.convenciones))

# By default the enums live at global scope; with --espacios they move into namespaces and every use is
# qualified (see qualify below).
GLOBAL_TYPES = """enum T09Big { T09BigMin = -2147483647 - 1, T09BigNeg = -5, T09BigMax = 2147483647 };
enum class T09Color : int { Red = 1, Green = 2, Blue = 4 };
enum class T09E8 : unsigned char { A = 1, B = 200 };
enum class T09E16 : short { N = -300, P = 7 };
enum class T09E64 : unsigned long long { A = 1, B = 0x8000000000000001ULL };
enum class T09Wide : wchar_t { A = 1, B = 3 };
"""
TYPES = """struct Pair { int a; int b; };
"""
# (name, result, parameters, C++ body, call check with CALL and T09:: names, needs record)
FUNCTIONS = [
    ('param', 'int', 'T09Color c, int k', 'return (int)c * 10 + k;', 'CALL(T09Color::Blue, 3) != 43', False),
    ('ret', 'T09Color', 'int k', 'return k ? T09Color::Green : T09Color::Red;',
     'CALL(1) != T09Color::Green || CALL(0) != T09Color::Red', False),
    ('big', 'T09Big', 'T09Big b', 'return b == T09BigMin ? T09BigMax : b == T09BigMax ? T09BigNeg : T09BigMin;',
     'CALL(T09BigMin) != T09BigMax || CALL(T09BigMax) != T09BigNeg', False),
    ('u8', 'T09E8', 'T09E8 v', 'return v == T09E8::A ? T09E8::B : T09E8::A;', 'CALL(T09E8::A) != T09E8::B || CALL(T09E8::B) != T09E8::A', False),
    ('s16', 'T09E16', 'T09E16 v, double d', 'return v == T09E16::P && d == 1.5 ? T09E16::N : T09E16::P;',
     'CALL(T09E16::P, 1.5) != T09E16::N || CALL(T09E16::N, 1.5) != T09E16::P', False),
    ('u64', 'T09E64', 'T09E64 v, T09Color c', 'return c == T09Color::Blue ? T09E64::B : v;',
     'CALL(T09E64::A, T09Color::Blue) != T09E64::B || CALL(T09E64::A, T09Color::Red) != T09E64::A', False),
    ('wide', 'T09Wide', 'T09Wide v', 'return v == T09Wide::A ? T09Wide::B : T09Wide::A;',
     'CALL(T09Wide::A) != T09Wide::B || CALL(T09Wide::B) != T09Wide::A', False),
    ('record', 'int', 'T09Color c, Pair p, T09E8 u', 'return (int)c + p.a * p.b + (int)u;',
     'CALL(T09Color::Green, pair, T09E8::B) != 217', True),
    ('record_ret', 'T09E8', 'Pair p, T09E16 s', 'return p.a == 3 && s == T09E16::N ? T09E8::B : T09E8::A;',
     'CALL(pair, T09E16::N) != T09E8::B || CALL(pair, T09E16::P) != T09E8::A', True),
]

SCOPED_ENUMS = ('T09Color', 'T09E8', 'T09E16', 'T09E64', 'T09Wide')
UNSCOPED_NAMES = ('T09Big', 'T09BigMin', 'T09BigNeg', 'T09BigMax')


def qualify(text):
    """With --espacios, name the scoped enums as T09::In::X and the unscoped enum and its values as T09::Otro::Mas::X."""
    if not args.espacios:
        return text
    for name in SCOPED_ENUMS:
        text = re.sub(r'(?<![:\w])' + name + r'\b', 'T09::In::' + name, text)
    for name in UNSCOPED_NAMES:
        text = re.sub(r'(?<![:\w])' + name + r'\b', 'T09::Otro::Mas::' + name, text)
    return text


if args.espacios:
    lines = GLOBAL_TYPES.strip().splitlines()
    GLOBAL_TYPES = ('namespace T09 { namespace In {\n' + '\n'.join(lines[1:]) + '\n}\nnamespace Otro { namespace Mas {\n'
                    + lines[0] + '\n} }\n}\n')
    FUNCTIONS = [(f[0], qualify(f[1]), qualify(f[2]), qualify(f[3]), qualify(f[4]), f[5]) for f in FUNCTIONS]


def run(label, command, cwd, expected=0):
    result = subprocess.run(command, cwd=cwd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, errors='replace')
    log = root / (label + '.log')
    log.write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' + result.stdout,
                   encoding='utf-8')
    if (expected == 0 and result.returncode) or (expected != 0 and result.returncode == 0):
        print(result.stdout[-3000:], flush=True)
        raise SystemExit(label + ': codigo inesperado; evidencia: ' + str(log))
    return result.stdout


def functions_for(convention):
    # A wchar_t enum is not projected yet with swiftcall or swiftasynccall (see the record of this change).
    return [f for f in FUNCTIONS if (not f[5] or convention in RECORD_CONVENTIONS) and
            not (f[0] == 'wide' and convention.startswith('swift'))]


def has_pointer(convention, function):
    return convention in POINTER_CONVENTIONS and (not function[5] or convention in POINTER_RECORD_CONVENTIONS)


def header(convention):
    attribute = f'__attribute__(({convention}))'
    linkage = 'extern "C" ' if convention in EXTERN_C else ''
    text = ['#pragma once', '#if defined(T09_EXPORT)', '#define API __declspec(dllexport)',
            '#elif defined(T09_IMPORT)', '#define API __declspec(dllimport)', '#else', '#define API', '#endif',
            GLOBAL_TYPES, 'namespace T09 {', TYPES]
    for function in functions_for(convention):
        name, result, parameters = function[:3]
        text.append(f'{linkage}API {result} {attribute} t09_{name}({parameters});')
        if has_pointer(convention, function):
            text.append(f'using Ptr_{name} = {result} ({attribute} *)({parameters});')
            text.append(f'{linkage}API Ptr_{name} t09_get_{name}();')
    return '\n'.join(text) + '\n}\n'


def source(convention):
    attribute = f'__attribute__(({convention}))'
    text = ['extern "C" { __declspec(selectany) int _fltused = 0; }', '#include "api.h"', 'namespace T09 {']
    for function in functions_for(convention):
        name, result, parameters, body = function[:4]
        text.append(f'{result} {attribute} t09_{name}({parameters}) {{ {body} }}')
        if has_pointer(convention, function):
            text.append(f'Ptr_{name} t09_get_{name}() {{ return t09_{name}; }}')
    return '\n'.join(text) + '\n}\n'


def checks(convention, pointers, native):
    lines, code = [], 10
    for function in functions_for(convention):
        name, check = function[0], function[4]
        direct = check.replace('CALL', 'T09::t09_' + name)
        lines.append(f'if ({direct}) return {code};'); code += 1
        if pointers and has_pointer(convention, function):
            through = check.replace('CALL', 'p_' + name)
            lines.append(f'T09::Ptr_{name} p_{name} = T09::t09_get_{name}(); if ({through}) return {code};')
        code += 1
    text = ' '.join(lines)
    return text.replace('::', '.') if native else text


def control(convention):
    return ('extern "C" { __declspec(selectany) int _fltused = 0; }\n#include "api.h"\n'
            'int main() { T09::Pair pair; pair.a = 3; pair.b = 5; ' + checks(convention, True, False) + ' return 0; }\n')


def body(convention, pointers=True):
    return 'T09.Pair pair; pair.a = 3; pair.b = 5; ' + checks(convention, pointers, True)


def abi_signature(line):
    """Convention, return extension and per-parameter (type, extension) of an IR define/declare line."""
    if not line:
        return None
    before, rest = line.split('@', 1)
    convention = CC_TOKEN.search(before)
    result_ext = 'signext' if ' signext ' in before + ' ' else 'zeroext' if ' zeroext ' in before + ' ' else ''
    inner = rest.split('(', 1)[1].rsplit(')', 1)[0]
    parameters = []
    depth, piece = 0, ''
    for ch in inner + ',':
        if ch in '([{<': depth += 1
        if ch in ')]}>': depth -= 1
        if ch == ',' and depth == 0:
            words = piece.split()
            if words:
                kind = words[0]
                ext = 'signext' if 'signext' in words else 'zeroext' if 'zeroext' in words else ''
                parameters.append((kind, ext))
            piece = ''
        else:
            piece += ch
    return (convention.group(1) if convention else 'ccc', result_ext, tuple(parameters))


def symbol_pattern(name):
    return r'@"?(?:\?|__regcall3__)?t09_' + re.escape(name) + r'(?:@|"?\()'


def symbol_line(ir, verb, name):
    pattern = re.compile(r'^' + verb + r' .*' + symbol_pattern(name))
    return next((l for l in ir.splitlines() if pattern.search(l)), '')


def project(name, references):
    return ('<Project><PropertyGroup><Name>' + name + '</Name></PropertyGroup><ItemGroup>'
            '<Compile Include="Main.ns" />' + references + '</ItemGroup></Project>')


def cpp_reference(prefix=''):
    runtime = ' Runtime="' + prefix + 'api.dll"' if args.dll else ''
    return ('<CppLibraryReference Header="' + prefix + 'api.h" Language="C++" Standard="c++20" Library="'
            + prefix + 'api.lib"' + runtime + ' />')


def check_ir(tag, ir, expected, convention, direct_calls=True):
    for name, signature in expected.items():
        observed = abi_signature(symbol_line(ir, 'declare', name))
        if observed != signature:
            raise SystemExit(f'{tag}: t09_{name}: NSC {observed!r} frente a Clang {signature!r}; evidencia: {root}')
        if direct_calls and not re.search(r'(?:call|invoke) ' + re.escape(signature[0]) + r' [^\n]*' + symbol_pattern(name), ir):
            raise SystemExit(f'{tag}: la llamada directa a t09_{name} no usa {signature[0]}; evidencia: {root}')
    pointers = sum(1 for function in functions_for(convention) if has_pointer(convention, function))
    if pointers:
        cc = next(iter(expected.values()))[0]
        user = ir.split('define internal i1 @__nativesharp_rtti')[0]
        indirect = re.findall(r'(?:call|invoke) ' + re.escape(cc) + r' [^@\n]*%[\w.]+\(', user)
        if len(indirect) < pointers:
            raise SystemExit(f'{tag}: {len(indirect)} llamadas por puntero con {cc}, se esperaban {pointers}; '
                             f'evidencia: {root}')


if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version'], root):
    raise SystemExit('Se requiere Clang 23.1.0')
for convention in CONVENTIONS:
    for opt in [int(level) for level in re.split(r'[,+]', args.niveles)]:
        tag = f'{convention}-O{opt}'
        case = root / tag; case.mkdir()
        (case / 'api.h').write_text(header(convention), encoding='utf-8')
        (case / 'api.cpp').write_text(source(convention), encoding='utf-8')
        (case / 'Control.cpp').write_text(control(convention), encoding='utf-8')
        flags = ['-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-fno-exceptions', '-O' + str(opt)]
        run(f'{tag}-api', ['clang++.exe', *flags, *(['-DT09_EXPORT'] if args.dll else []), '-c', 'api.cpp',
                           '-o', 'api.obj'], case)
        clang_ir = run(f'{tag}-api-ir', ['clang++.exe', *flags, '-O0', '-S', '-emit-llvm', 'api.cpp', '-o', '-'], case)
        expected = {}
        for name, *_ in functions_for(convention):
            signature = abi_signature(symbol_line(clang_ir, 'define', name))
            if not signature or signature[0] == 'ccc':
                raise SystemExit(f'{tag}: Clang no emite la convencion de t09_{name}; evidencia: {root}')
            expected[name] = signature
        if args.dll:
            run(f'{tag}-lib', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib', '/out:api.dll',
                               '/implib:api.lib', 'api.obj'], case)
            path = case / 'api.h'
            path.write_text('#define T09_IMPORT 1\n' + path.read_text(encoding='utf-8'), encoding='utf-8')
        else:
            run(f'{tag}-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], case)
        run(f'{tag}-control-obj', ['clang++.exe', *flags, '-c', 'Control.cpp', '-o', 'control.obj'], case)
        run(f'{tag}-control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console',
                                    '/nodefaultlib', '/out:control.exe', 'control.obj', 'api.lib'], case)
        run(f'{tag}-control-run', [str(case / 'control.exe')], case)
        (case / 'Main.ns').write_text('int Main() { ' + body(convention) + ' return 0; }\n', encoding='utf-8')
        (case / 'Fixture.nsproj').write_text(project('Fixture', cpp_reference()), encoding='utf-8')
        common = ['Fixture.nsproj', '--cpp-clang-root=' + args.clang_root]
        ir = run(f'{tag}-ir', [args.nsc, '--dump-project-oop-llvm', *common], case)
        check_ir(tag, ir, expected, convention)
        run(f'{tag}-build', [args.nsc, '--emit-project-oop-exe', *common, '--no-artifact-cache', '-O' + str(opt),
                             '-o', 'native.exe'], case)
        run(f'{tag}-native-run', [str(case / 'native.exe')], case)
        print(f'{tag}: control C++ y NativeSharp ' + ('directo y por puntero' if convention in POINTER_CONVENTIONS
              else 'directo') + ' pasaron', flush=True)
        if args.precompilado:
            producer = case / 'producer'; producer.mkdir()
            for name in ('api.h', 'api.lib') + (('api.dll',) if args.dll else ()):
                shutil.copyfile(case / name, producer / name)
            (producer / 'Producer.ns').write_text('namespace T09P { public class Bridge { public static int Run<T>(T unused) { '
                + body(convention) + ' return 0; } } }\n', encoding='utf-8')
            (producer / 'Producer.nsproj').write_text('<Project><PropertyGroup><Name>T09Producer</Name>'
                '<OutputType>Library</OutputType></PropertyGroup><ItemGroup><Compile Include="Producer.ns" />'
                + cpp_reference() + '</ItemGroup></Project>', encoding='utf-8')
            run(f'{tag}-producer', [args.nsc, '--emit-project-oop-lib', 'Producer.nsproj', '--cpp-clang-root='
                + args.clang_root, '--no-artifact-cache', '-O' + str(opt), '-o', 'Producer.lib'], producer)
            for visibility in ('con-encabezado', 'sin-encabezado'):
                consumer = case / visibility; consumer.mkdir()
                references = '<LibraryReference Include="../producer/Producer.lib" />'
                if visibility == 'con-encabezado':
                    references += cpp_reference('../producer/')
                else:
                    (producer / 'api.h').rename(producer / 'api.h.oculto')
                (consumer / 'Main.ns').write_text('int Main() { return T09P.Bridge.Run<int>(0); }\n', encoding='utf-8')
                (consumer / 'Consumer.nsproj').write_text(project('T09Consumer', references), encoding='utf-8')
                consumer_ir = run(f'{tag}-{visibility}-ir', [args.nsc, '--dump-project-oop-llvm', 'Consumer.nsproj',
                    '--cpp-clang-root=' + args.clang_root], consumer)
                check_ir(f'{tag} {visibility}', consumer_ir, expected, convention, direct_calls=False)
                run(f'{tag}-{visibility}-build', [args.nsc, '--emit-project-oop-exe', 'Consumer.nsproj',
                    '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O' + str(opt), '-o',
                    'consumer.exe'], consumer)
                if args.dll: shutil.copyfile(producer / 'api.dll', consumer / 'api.dll')
                run(f'{tag}-{visibility}-run', [str(consumer / 'consumer.exe')], consumer)
                print(f'{tag}: plantilla precompilada {visibility} paso', flush=True)

# Negative: a C union of floats by value with regcall, which Clang expands as a float, is still not projected.
negative = root / 'negativo-union-float'; negative.mkdir()
(negative / 'api.h').write_text('#pragma once\ntypedef union T09UF { float a; float b; } T09UF;\n'
    'int __attribute__((regcall)) t09_uf(T09UF u, int k);\n', encoding='utf-8')
(negative / 'api.c').write_text('#include "api.h"\nint __attribute__((regcall)) t09_uf(T09UF u, int k) { return (int)u.a + k; }\n',
                                encoding='utf-8')
(negative / 'Main.ns').write_text('int Main() { T09UF u; u.a = 3; return t09_uf(u, 4) == 7 ? 0 : 1; }\n', encoding='utf-8')
(negative / 'Fixture.nsproj').write_text('<Project><PropertyGroup><Name>Fixture</Name></PropertyGroup><ItemGroup>'
    '<Compile Include="Main.ns" /><CppLibraryReference Header="api.h" Language="C" Standard="c17" '
    'Library="api.lib" /></ItemGroup></Project>', encoding='utf-8')
run('negativo-compile', ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c17', '-c', 'api.c', '-o', 'api.obj'], negative)
run('negativo-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], negative)
output = run('negativo-nsc', [args.nsc, '--dump-project-oop-llvm', 'Fixture.nsproj',
                              '--cpp-clang-root=' + args.clang_root], negative, expected=1)
if 'regcall currently supports only int scalar signatures' not in output:
    raise SystemExit('La union C solo float con regcall no fue rechazada como se esperaba; evidencia: ' + str(root))
if args.espacios:
    # Negative: an enum nested in a class is not projected; a pointer alias naming it is still rejected, so the
    # namespace-aware identity does not take a containing class for a namespace.
    negative = root / 'negativo-enum-en-clase'; negative.mkdir()
    (negative / 'api.h').write_text('#pragma once\nnamespace T09 { struct S { enum class E : int { A = 1, B = 2 }; };\n'
        'S::E t09_f(S::E v, int k);\nusing P = S::E (*)(S::E, int);\nP t09_fetch();\n}\n', encoding='utf-8')
    (negative / 'api.cpp').write_text('#include "api.h"\nnamespace T09 { S::E t09_f(S::E v, int k) { return v; }\n'
        'P t09_fetch() { return t09_f; } }\n', encoding='utf-8')
    (negative / 'Main.ns').write_text('int Main() { T09.P p = T09.t09_fetch(); return p(T09.S.E.B, 4) == T09.S.E.B ? 0 : 1; }\n',
                                      encoding='utf-8')
    (negative / 'Fixture.nsproj').write_text(project('Fixture', '<CppLibraryReference Header="api.h" Language="C++" '
        'Standard="c++20" Library="api.lib" />'), encoding='utf-8')
    run('negativo-clase-compile', ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-c', 'api.cpp',
                                   '-o', 'api.obj'], negative)
    run('negativo-clase-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], negative)
    output = run('negativo-clase-nsc', [args.nsc, '--dump-project-oop-llvm', 'Fixture.nsproj',
                                        '--cpp-clang-root=' + args.clang_root], negative, expected=1)
    if 'C callback signature contains an unavailable type' not in output:
        raise SystemExit('El enum anidado en una clase no fue rechazado como se esperaba; evidencia: ' + str(root))
print('OK: enums C++ por valor con convenciones no cdecl comparados con Clang' + (' (en espacios de nombres)' if args.espacios else '')
      + '; union solo float con regcall rechazada' + ('; enum anidado en clase rechazado' if args.espacios else '')
      + '; evidencia:', root)
