"""T09-003: records C++ por valor en punteros a funcion regcall, comparados con Clang 23.1.0.

Clang pasa con regcall un record C++ trivial de 1, 2, 4 u 8 bytes que no es un agregado homogeneo
flotante como un entero de su tamano, y uno mayor por referencia oculta; lo devuelve igual (entero
o sret). Para records de esos tamanos compila funciones C++ regcall en un espacio de nombres, con el
record como argumento, junto a escalares o a otro record, y como retorno, tambien con retorno enum
junto a un record entero o a un agregado flotante expandido, y un alias de puntero a funcion con
cada firma. Un control C++ compilado por Clang y un consumidor NativeSharp ejecutan las
mismas comprobaciones, O0-O3, en biblioteca estatica y con --dll en DLL; con --precompilado tambien
una plantilla consumida con y sin encabezado. Exige en el IR de NSC los tipos fisicos y las
extensiones signext/zeroext de cada parametro y del retorno identicos a la definicion de Clang,
y x86_regcallcc en cada declaracion y llamada directa y en todas las llamadas por puntero. El control negativo conserva el rechazo de un
puntero regcall con un struct C++ con un campo de bits de ancho cero entre dos float, que Clang
expande y NSC no representa.
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
parser.add_argument('--niveles', default='0,1,2,3')
args = parser.parse_args()
root = Path(tempfile.mkdtemp(prefix='nsc-t09-cpp-regcall-puntero-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)

TYPES = """struct R8 { int a; int b; };
struct R4 { short a; short b; };
struct R2 { unsigned char a; unsigned char b; };
struct R1 { signed char c; };
struct RI { int a; int b; int c; int d; int e; };
struct RF { float x; float y; };
enum class E8 : unsigned char { X = 1, Y = 200 };
enum class E16 : short { N = -300, P = 7 };
"""
ATTR = '__attribute__((regcall))'
# (name, result, parameters, C++ body, call check with CALL; RES names a returned record)
FUNCTIONS = [
    ('p8', 'int', 'R8 r, int k', 'return r.a * 10 + r.b + k;', 'CALL(r8, 1) != 35'),
    ('p4', 'long long', 'int k, R4 r', 'return (long long)r.a * 100 + r.b + k;', 'CALL(2, r4) != (long)508'),
    ('p21', 'int', 'R2 r, R1 s', 'return r.a + r.b * 10 + s.c * 100;', 'CALL(r2, r1) != -170'),
    ('pi', 'int', 'RI r, R8 s', 'return r.a + r.e + s.a * s.b;', 'CALL(ri, r8) != 18'),
    ('pd', 'double', 'double d, R8 r', 'return d * r.a + r.b;', 'CALL(1.5, r8) != 8.5'),
    ('r8', 'R8', 'R8 r, int k', 'R8 v; v.a = r.a + k; v.b = r.b * k; return v;', 'R8 RES = CALL(r8, 2); | RES.a != 5 || RES.b != 8'),
    ('r4', 'R4', 'R4 r', 'R4 v; v.a = r.b; v.b = r.a; return v;', 'R4 RES = CALL(r4); | RES.a != 6 || RES.b != 5'),
    ('ri', 'RI', 'int k, R1 s', 'RI v; v.a = k; v.b = 0; v.c = 0; v.d = 0; v.e = s.c; return v;',
     'RI RES = CALL(9, r1); | RES.a != 9 || RES.e != -2'),
    ('pe', 'E8', 'R8 r, int k', 'return r.a + k == 4 ? E8::Y : E8::X;', 'CALL(r8, 1) != T09::E8::Y || CALL(r8, 2) != T09::E8::X'),
    ('pfe', 'E16', 'RF r, E8 e', 'return r.x + r.y == 7 && e == E8::Y ? E16::N : E16::P;',
     'CALL(rf, T09::E8::Y) != T09::E16::N || CALL(rf, T09::E8::X) != T09::E16::P'),
]
SETUP = ('T09::R8 r8; r8.a = 3; r8.b = 4; T09::R4 r4; r4.a = 5; r4.b = 6; T09::R2 r2; r2.a = 10; r2.b = 2; '
         'T09::R1 r1; r1.c = -2; T09::RI ri; ri.a = 1; ri.b = 0; ri.c = 0; ri.d = 0; ri.e = 5; T09::RF rf; rf.x = 3; rf.y = 4;')


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


def header():
    text = ['#pragma once', '#if defined(T09_EXPORT)', '#define API __declspec(dllexport)',
            '#elif defined(T09_IMPORT)', '#define API __declspec(dllimport)', '#else', '#define API', '#endif',
            'namespace T09 {', TYPES]
    for name, result, parameters, *_ in FUNCTIONS:
        text.append(f'API {result} {ATTR} t09_{name}({parameters});')
        text.append(f'using Ptr_{name} = {result} ({ATTR} *)({parameters});')
        text.append(f'API Ptr_{name} t09_get_{name}();')
    return '\n'.join(text) + '\n}\n'


def source():
    text = ['extern "C" { __declspec(selectany) int _fltused = 0; }', '#include "api.h"', 'namespace T09 {']
    for name, result, parameters, body, _ in FUNCTIONS:
        text.append(f'{result} {ATTR} t09_{name}({parameters}) {{ {body} }}')
        text.append(f'Ptr_{name} t09_get_{name}() {{ return t09_{name}; }}')
    return '\n'.join(text) + '\n}\n'


def checks(native):
    lines, code = [], 10
    for name, result, _, _, check in FUNCTIONS:
        for callee, pre in (('T09::t09_' + name, ''), ('p_' + name, f'T09::Ptr_{name} p_{name} = T09::t09_get_{name}(); ')):
            text = check.replace('CALL', callee)
            if '; |' in text:
                declaration, condition = (part.strip() for part in text.split('; |', 1))
                declaration += ';'
                declaration = declaration.replace(result + ' RES', 'T09::' + result + ' RES_' + str(code))
                condition = condition.replace('RES', 'RES_' + str(code))
                lines.append(f'{pre}{declaration} if ({condition}) return {code};')
            else:
                lines.append(f'{pre}if ({text}) return {code};')
            code += 1
    joined = ' '.join(lines)
    return joined.replace('::', '.') if native else joined


def control():
    return ('extern "C" { __declspec(selectany) int _fltused = 0; }\n#include "api.h"\n'
            'int main() { ' + SETUP + ' ' + checks(False) + ' return 0; }\n')


def body():
    return SETUP.replace('::', '.') + ' ' + checks(True)


def physical_signature(line):
    if not line:
        return None
    before, rest = line.split('@', 1)
    cc = 'x86_regcallcc' if 'x86_regcallcc' in before else 'ccc'
    words = before.split()
    result_ext = 'signext' if 'signext' in words else 'zeroext' if 'zeroext' in words else ''
    result = [w for w in words if w not in ('noundef', 'signext', 'zeroext', 'dso_local', 'dllimport', 'dllexport')][-1]
    inner = rest.split('(', 1)[1].rsplit(')', 1)[0]
    parameters, depth, piece = [], 0, ''
    for ch in inner + ',':
        if ch in '([{<': depth += 1
        if ch in ')]}>': depth -= 1
        if ch == ',' and depth == 0:
            words = piece.split()
            if words: parameters.append(words[0] + (' signext' if 'signext' in words else ' zeroext' if 'zeroext' in words else ''))
            piece = ''
        else:
            piece += ch
    return cc, result_ext + ' ' + result, tuple(parameters)


def symbol_line(ir, verb, name):
    pattern = re.compile(r'^' + verb + r' .*@"\?t09_' + re.escape(name) + r'@T09@@')
    return next((l for l in ir.splitlines() if pattern.search(l)), '')


def check_ir(tag, ir, expected, direct_calls=True):
    for name, signature in expected.items():
        observed = physical_signature(symbol_line(ir, 'declare', name))
        if observed != signature:
            raise SystemExit(f'{tag}: t09_{name}: NSC {observed!r} frente a Clang {signature!r}; evidencia: {root}')
        if direct_calls and not re.search(r'(?:call|invoke) x86_regcallcc [^\n]*@"\?t09_' + re.escape(name) + r'@T09@@', ir):
            raise SystemExit(f'{tag}: la llamada directa a t09_{name} no usa x86_regcallcc; evidencia: {root}')
    user = ir.split('define internal i1 @__nativesharp_rtti')[0]
    indirect = re.findall(r'(?:call|invoke) x86_regcallcc [^@\n]*%[\w.]+\(', user)
    if len(indirect) < len(FUNCTIONS):
        raise SystemExit(f'{tag}: {len(indirect)} llamadas por puntero con x86_regcallcc, se esperaban {len(FUNCTIONS)}; '
                         f'evidencia: {root}')


def project(name, references):
    return ('<Project><PropertyGroup><Name>' + name + '</Name></PropertyGroup><ItemGroup>'
            '<Compile Include="Main.ns" />' + references + '</ItemGroup></Project>')


def cpp_reference(prefix=''):
    runtime = ' Runtime="' + prefix + 'api.dll"' if args.dll else ''
    return ('<CppLibraryReference Header="' + prefix + 'api.h" Language="C++" Standard="c++20" Library="'
            + prefix + 'api.lib"' + runtime + ' />')


if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version'], root):
    raise SystemExit('Se requiere Clang 23.1.0')
for opt in [int(level) for level in re.split(r'[,+]', args.niveles)]:
    tag = f'regcall-O{opt}'
    case = root / tag; case.mkdir()
    (case / 'api.h').write_text(header(), encoding='utf-8')
    (case / 'api.cpp').write_text(source(), encoding='utf-8')
    (case / 'Control.cpp').write_text(control(), encoding='utf-8')
    flags = ['-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-fno-exceptions', '-O' + str(opt)]
    run(f'{tag}-api', ['clang++.exe', *flags, *(['-DT09_EXPORT'] if args.dll else []), '-c', 'api.cpp', '-o', 'api.obj'], case)
    clang_ir = run(f'{tag}-api-ir', ['clang++.exe', *flags, '-O0', '-S', '-emit-llvm', 'api.cpp', '-o', '-'], case)
    expected = {}
    for name, *_ in FUNCTIONS:
        signature = physical_signature(symbol_line(clang_ir, 'define', name))
        if not signature or signature[0] != 'x86_regcallcc':
            raise SystemExit(f'{tag}: Clang no emite regcall para t09_{name}; evidencia: {root}')
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
    (case / 'Main.ns').write_text('int Main() { ' + body() + ' return 0; }\n', encoding='utf-8')
    (case / 'Fixture.nsproj').write_text(project('Fixture', cpp_reference()), encoding='utf-8')
    common = ['Fixture.nsproj', '--cpp-clang-root=' + args.clang_root]
    ir = run(f'{tag}-ir', [args.nsc, '--dump-project-oop-llvm', *common], case)
    check_ir(tag, ir, expected)
    run(f'{tag}-build', [args.nsc, '--emit-project-oop-exe', *common, '--no-artifact-cache', '-O' + str(opt),
                         '-o', 'native.exe'], case)
    run(f'{tag}-native-run', [str(case / 'native.exe')], case)
    print(f'{tag}: control C++ y NativeSharp directo y por puntero pasaron', flush=True)
    if args.precompilado:
        producer = case / 'producer'; producer.mkdir()
        for name in ('api.h', 'api.lib') + (('api.dll',) if args.dll else ()):
            shutil.copyfile(case / name, producer / name)
        (producer / 'Producer.ns').write_text('namespace T09P { public class Bridge { public static int Run<T>(T unused) { '
            + body() + ' return 0; } } }\n', encoding='utf-8')
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
            check_ir(f'{tag} {visibility}', consumer_ir, expected, direct_calls=False)
            run(f'{tag}-{visibility}-build', [args.nsc, '--emit-project-oop-exe', 'Consumer.nsproj',
                '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O' + str(opt), '-o',
                'consumer.exe'], consumer)
            if args.dll: shutil.copyfile(producer / 'api.dll', consumer / 'api.dll')
            run(f'{tag}-{visibility}-run', [str(consumer / 'consumer.exe')], consumer)
            print(f'{tag}: plantilla precompilada {visibility} paso', flush=True)

# Negative: a C++ struct with a zero-width bitfield between two floats, which Clang expands as two floats, is
# still rejected through a regcall pointer: NSC does not represent bitfields in an expansion.
negative = root / 'negativo-campo-ancho-cero'; negative.mkdir()
(negative / 'api.h').write_text('#pragma once\nnamespace T09 {\nstruct RZ { float a; int : 0; float b; };\n'
    'int __attribute__((regcall)) t09_z(RZ v, int k);\nusing Ptr_z = int (__attribute__((regcall)) *)(RZ, int);\n'
    'Ptr_z t09_get_z();\n}\n', encoding='utf-8')
(negative / 'api.cpp').write_text('#include "api.h"\nnamespace T09 {\nint __attribute__((regcall)) t09_z(RZ v, int k) '
    '{ return (int)(v.a + v.b) + k; }\nPtr_z t09_get_z() { return t09_z; }\n}\n', encoding='utf-8')
(negative / 'Main.ns').write_text('int Main() { T09.RZ v; v.a = 3; v.b = 0; T09.Ptr_z p = T09.t09_get_z(); '
    'return p(v, 4) == 7 ? 0 : 1; }\n', encoding='utf-8')
(negative / 'Fixture.nsproj').write_text(project('Fixture', '<CppLibraryReference Header="api.h" Language="C++" '
    'Standard="c++20" Library="api.lib" />'), encoding='utf-8')
cpp = ['-target', 'x86_64-pc-windows-msvc', '-std=c++20']
negative_ir = run('negativo-clang-ir', ['clang++.exe', *cpp, '-S', '-emit-llvm', 'api.cpp', '-o', '-'], negative)
if not re.search(r'x86_regcallcc noundef i32 @"\?t09_z@T09@@[^"]*"\(float %0, float %1, i32', negative_ir):
    raise SystemExit('Clang ya no expande el struct con campo de bits de ancho cero; evidencia: ' + str(root))
run('negativo-compile', ['clang++.exe', *cpp, '-c', 'api.cpp', '-o', 'api.obj'], negative)
run('negativo-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], negative)
output = run('negativo-nsc', [args.nsc, '--dump-project-oop-llvm', 'Fixture.nsproj',
                              '--cpp-clang-root=' + args.clang_root], negative, expected=1)
if 'regcall function pointer signature is not yet supported' not in output:
    raise SystemExit('El puntero regcall con campo de bits de ancho cero no fue rechazado como se esperaba; evidencia: '
                     + str(root))
print('OK: records C++ por valor en punteros regcall comparados con Clang; puntero con campo de ancho cero rechazado; '
      'evidencia:', root)
