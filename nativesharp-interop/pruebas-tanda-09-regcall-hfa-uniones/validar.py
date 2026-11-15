"""T09-003: agregados homogeneos C de uno a cuatro flotantes y uniones flotantes por valor con regcall.

Clang 23.1.0 expande con regcall un agregado homogeneo de float o double de una a cuatro piezas,
tambien una union, que expande como su primer campo de mayor tamano. Para uniones de float y de
double, uniones con un arreglo o un struct como campo mayor y structs de un solo float o double, se
compilan funciones C regcall que reciben el record en primera posicion, entre escalares o junto a
otro record, devuelven int, float o double, en llamada directa y por puntero a funcion. Tambien
funciones que devuelven esos records (uniones de float y de double, structs de una pieza, un struct
de dos float y uniones de tres y cuatro piezas) a partir de escalares o de otro record, que Clang
devuelve directamente como un struct LLVM de sus piezas. Un control C
compilado por Clang y un consumidor NativeSharp ejecutan las mismas comprobaciones, O0-O3, en
biblioteca estatica y con --dll en DLL; con --precompilado tambien una plantilla consumida con y
sin encabezado. Exige en el IR de NSC los tipos fisicos de cada parametro y del retorno identicos a
la definicion de Clang, la convencion x86_regcallcc en cada declaracion y llamada directa y en las
llamadas por puntero. El control negativo conserva el rechazo de un struct C++ con un campo de bits
de ancho cero entre dos float, que Clang omite al expandir y NSC no representa.
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
root = Path(tempfile.mkdtemp(prefix='nsc-t09-regcall-hfa-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)

TYPES = """typedef struct T09Par { float x; float y; } T09Par;
typedef struct T09XYZ { float x; float y; float z; } T09XYZ;
typedef union T09U1 { float a; float b; } T09U1;
typedef union T09U2 { float a[2]; float b; } T09U2;
typedef union T09UD { double a; double b; } T09UD;
typedef union T09US { T09XYZ s; float a; } T09US;
typedef union T09U4 { float a[4]; T09Par s; } T09U4;
typedef struct T09S1 { float a; } T09S1;
typedef struct T09S1D { double a; } T09S1D;
"""
# (name, result, parameters, C body, call check with CALL, NativeSharp setup, C setup)
SETUP = ('T09U1 u1; u1.a = 3; T09U2 u2; u2.a[0] = 2; u2.a[1] = 5; T09UD ud; ud.a = 1.5; '
         'T09US us; us.s.x = 1; us.s.y = 2; us.s.z = 4; T09U4 u4; u4.a[0] = 1; u4.a[1] = 2; u4.a[2] = 3; u4.a[3] = 4; '
         'T09S1 s1; s1.a = 6; T09S1D s1d; s1d.a = 2.25; T09Par p; p.x = 7; p.y = 8;')
FUNCTIONS = [
    ('u1', 'int', 'T09U1 u, int k', 'return (int)u.b + k;', 'CALL(u1, 4) != 7'),
    ('u2', 'int', 'T09U2 u, int k', 'return (int)(u.a[0] * 10 + u.a[1]) + k;', 'CALL(u2, 1) != 26'),
    ('ud', 'double', 'int k, T09UD u, double d', 'return u.b * k + d;', 'CALL(2, ud, 0.5) != 3.5'),
    ('us', 'int', 'T09US u', 'return (int)(u.s.x + u.s.y * 10 + u.s.z * 100);', 'CALL(us) != 421'),
    ('u4', 'int', 'T09U4 u, T09Par q', 'return (int)(u.a[0] + u.a[1] * 10 + u.a[2] * 100 + u.a[3] * 1000 + q.x + q.y);',
     'CALL(u4, p) != 4336'),
    ('s1', 'int', 'int k, T09S1 s, int j', 'return (int)s.a * k + j;', 'CALL(2, s1, 3) != 15'),
    ('s1d', 'double', 'T09S1D s, T09U1 u', 'return s.a + u.a;', 'CALL(s1d, u1) != 5.25'),
    ('mix', 'float', 'T09S1 s, T09UD u', 'return s.a * 100 + (float)u.a * 10;', 'CALL(s1, ud) != 615'),
    ('gu1', 'T09U1', 'float x', 'T09U1 r; r.a = x; return r;', 'CALL(3).b != 3'),
    ('gud', 'T09UD', 'double d, int k', 'T09UD r; r.a = d * k; return r;', 'CALL(1.5, 2).b != 3'),
    ('gs1', 'T09S1', 'int k', 'T09S1 r; r.a = (float)k; return r;', 'CALL(9).a != 9'),
    ('gs1d', 'T09S1D', 'T09S1 s', 'T09S1D r; r.a = s.a * 2; return r;', 'CALL(s1).a != 12'),
    ('gp', 'T09Par', 'float x, float y', 'T09Par r; r.x = x; r.y = y; return r;', 'CALL(7, 8).y != 8'),
    ('gu4', 'T09U4', 'T09Par q', 'T09U4 r; r.a[0] = q.x; r.a[1] = q.y; r.a[2] = 1; r.a[3] = 2; return r;',
     'CALL(p).a[1] != 8'),
    ('gus', 'T09US', 'T09U1 u, int k', 'T09US r; r.s.x = u.a; r.s.y = (float)k; r.s.z = 3; return r;',
     'CALL(u1, 5).s.y != 5'),
]


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


ATTR = '__attribute__((regcall))'


def header():
    text = ['#pragma once', '#if defined(T09_EXPORT)', '#define API __declspec(dllexport)',
            '#elif defined(T09_IMPORT)', '#define API __declspec(dllimport)', '#else', '#define API', '#endif', TYPES]
    for name, result, parameters, *_ in FUNCTIONS:
        text.append(f'API {result} {ATTR} t09_{name}({parameters});')
        text.append(f'typedef {result} ({ATTR} *Ptr_{name})({parameters});')
        text.append(f'API Ptr_{name} t09_get_{name}(void);')
    return '\n'.join(text) + '\n'


def source():
    text = ['__declspec(selectany) int _fltused = 0;', '#include "api.h"']
    for name, result, parameters, body, _ in FUNCTIONS:
        text.append(f'{result} {ATTR} t09_{name}({parameters}) {{ {body} }}')
        text.append(f'Ptr_{name} t09_get_{name}(void) {{ return t09_{name}; }}')
    return '\n'.join(text) + '\n'


def checks():
    lines, code = [], 10
    for name, *_, check in FUNCTIONS:
        lines.append(f'if ({check.replace("CALL", "t09_" + name)}) return {code};')
        lines.append(f'Ptr_{name} p_{name} = t09_get_{name}(); if ({check.replace("CALL", "p_" + name)}) return {code + 1};')
        code += 2
    return ' '.join(lines)


def control():
    return ('__declspec(selectany) int _fltused = 0;\n#include "api.h"\n'
            'int main(void) { ' + SETUP + ' ' + checks() + ' return 0; }\n')


def body():
    return SETUP + ' ' + checks()


def physical_signature(line):
    """Convention, return type and physical parameter types of an IR define/declare line."""
    if not line:
        return None
    before, rest = line.split('@', 1)
    cc = 'x86_regcallcc' if 'x86_regcallcc' in before else 'ccc'
    before = before.rstrip()
    result = before.split()[-1]
    if before.endswith('}'):
        depth, index = 0, len(before) - 1
        while index >= 0:
            depth += {'}': 1, '{': -1}.get(before[index], 0)
            if depth == 0: break
            index -= 1
        result = before[index:]
    inner = rest.split('(', 1)[1].rsplit(')', 1)[0]
    parameters, depth, piece = [], 0, ''
    for ch in inner + ',':
        if ch in '([{<': depth += 1
        if ch in ')]}>': depth -= 1
        if ch == ',' and depth == 0:
            words = piece.split()
            if words: parameters.append(words[0])
            piece = ''
        else:
            piece += ch
    return cc, result, tuple(parameters)


def symbol_line(ir, verb, name):
    pattern = re.compile(r'^' + verb + r' .*@"?__regcall3__t09_' + re.escape(name) + r'"?\(')
    return next((l for l in ir.splitlines() if pattern.search(l)), '')


TYPE_DEFINITIONS = {}


def normalize(signature):
    # Clang names the returned record type; NSC may spell the same physical structure literally.
    cc, result, parameters = signature
    for _ in range(16):
        expanded = re.sub(r'%[\w.]+', lambda m: TYPE_DEFINITIONS.get(m.group(0), m.group(0)), result)
        if expanded == result: break
        result = expanded
    return cc, ' '.join(result.replace('{', ' { ').replace('}', ' } ').replace(',', ' , ').split()), parameters


def check_ir(tag, ir, expected, direct_calls=True):
    for name, signature in expected.items():
        observed = physical_signature(symbol_line(ir, 'declare', name))
        if not observed or normalize(observed) != normalize(signature):
            raise SystemExit(f'{tag}: t09_{name}: NSC {observed!r} frente a Clang {signature!r}; evidencia: {root}')
        if direct_calls and not re.search(r'(?:call|invoke) x86_regcallcc [^\n]*@"?__regcall3__t09_' + re.escape(name) + r'"?\(', ir):
            raise SystemExit(f'{tag}: la llamada directa a t09_{name} no usa x86_regcallcc; evidencia: {root}')
    user = ir.split('define internal i1 @__nativesharp_rtti')[0]
    indirect = re.findall(r'(?:call|invoke) x86_regcallcc [^@\n]*%[\w.]+\(', user)
    if len(indirect) < len(FUNCTIONS):
        raise SystemExit(f'{tag}: {len(indirect)} llamadas por puntero con x86_regcallcc, se esperaban {len(FUNCTIONS)}; '
                         f'evidencia: {root}')


def project(name, references):
    return ('<Project><PropertyGroup><Name>' + name + '</Name></PropertyGroup><ItemGroup>'
            '<Compile Include="Main.ns" />' + references + '</ItemGroup></Project>')


def c_reference(prefix=''):
    runtime = ' Runtime="' + prefix + 'api.dll"' if args.dll else ''
    return ('<CppLibraryReference Header="' + prefix + 'api.h" Language="C" Standard="c17" Library="'
            + prefix + 'api.lib"' + runtime + ' />')


if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version'], root):
    raise SystemExit('Se requiere Clang 23.1.0')
for opt in [int(level) for level in re.split(r'[,+]', args.niveles)]:
    tag = f'regcall-O{opt}'
    case = root / tag; case.mkdir()
    (case / 'api.h').write_text(header(), encoding='utf-8')
    (case / 'api.c').write_text(source(), encoding='utf-8')
    (case / 'Control.c').write_text(control(), encoding='utf-8')
    flags = ['-target', 'x86_64-pc-windows-msvc', '-std=c17', '-O' + str(opt)]
    run(f'{tag}-api', ['clang.exe', *flags, *(['-DT09_EXPORT'] if args.dll else []), '-c', 'api.c', '-o', 'api.obj'], case)
    clang_ir = run(f'{tag}-api-ir', ['clang.exe', *flags, '-O0', '-S', '-emit-llvm', 'api.c', '-o', '-'], case)
    for match in re.finditer(r'^(%[\w.]+) = type (\{[^\n]*\})$', clang_ir, re.M):
        TYPE_DEFINITIONS[match.group(1)] = match.group(2)
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
    run(f'{tag}-control-obj', ['clang.exe', *flags, '-c', 'Control.c', '-o', 'control.obj'], case)
    run(f'{tag}-control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console',
                                '/nodefaultlib', '/out:control.exe', 'control.obj', 'api.lib'], case)
    run(f'{tag}-control-run', [str(case / 'control.exe')], case)
    (case / 'Main.ns').write_text('int Main() { ' + body() + ' return 0; }\n', encoding='utf-8')
    (case / 'Fixture.nsproj').write_text(project('Fixture', c_reference()), encoding='utf-8')
    common = ['Fixture.nsproj', '--cpp-clang-root=' + args.clang_root]
    ir = run(f'{tag}-ir', [args.nsc, '--dump-project-oop-llvm', *common], case)
    check_ir(tag, ir, expected)
    run(f'{tag}-build', [args.nsc, '--emit-project-oop-exe', *common, '--no-artifact-cache', '-O' + str(opt),
                         '-o', 'native.exe'], case)
    run(f'{tag}-native-run', [str(case / 'native.exe')], case)
    print(f'{tag}: control C y NativeSharp directo y por puntero pasaron', flush=True)
    if args.precompilado:
        producer = case / 'producer'; producer.mkdir()
        for name in ('api.h', 'api.lib') + (('api.dll',) if args.dll else ()):
            shutil.copyfile(case / name, producer / name)
        (producer / 'Producer.ns').write_text('namespace T09P { public class Bridge { public static int Run<T>(T unused) { '
            + body() + ' return 0; } } }\n', encoding='utf-8')
        (producer / 'Producer.nsproj').write_text('<Project><PropertyGroup><Name>T09Producer</Name>'
            '<OutputType>Library</OutputType></PropertyGroup><ItemGroup><Compile Include="Producer.ns" />'
            + c_reference() + '</ItemGroup></Project>', encoding='utf-8')
        run(f'{tag}-producer', [args.nsc, '--emit-project-oop-lib', 'Producer.nsproj', '--cpp-clang-root='
            + args.clang_root, '--no-artifact-cache', '-O' + str(opt), '-o', 'Producer.lib'], producer)
        for visibility in ('con-encabezado', 'sin-encabezado'):
            consumer = case / visibility; consumer.mkdir()
            references = '<LibraryReference Include="../producer/Producer.lib" />'
            if visibility == 'con-encabezado':
                references += c_reference('../producer/')
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

# Negative: Clang skips a zero-width bitfield between two floats of a C++ struct and expands two floats; NSC
# does not represent bitfields in an expansion, so the signature is still rejected.
negative = root / 'negativo-campo-ancho-cero'; negative.mkdir()
(negative / 'api.h').write_text('#pragma once\nstruct T09Z { float a; int : 0; float b; };\n'
    'int __attribute__((regcall)) t09_z(T09Z v, int k);\n', encoding='utf-8')
(negative / 'api.cpp').write_text('#include "api.h"\nint __attribute__((regcall)) t09_z(T09Z v, int k) '
    '{ return (int)(v.a + v.b) + k; }\n', encoding='utf-8')
(negative / 'Main.ns').write_text('int Main() { T09Z v; v.a = 3; v.b = 0; return t09_z(v, 4) == 7 ? 0 : 1; }\n',
                                  encoding='utf-8')
(negative / 'Fixture.nsproj').write_text(project('Fixture', '<CppLibraryReference Header="api.h" Language="C++" '
    'Standard="c++20" Library="api.lib" />'), encoding='utf-8')
cpp = ['-target', 'x86_64-pc-windows-msvc', '-std=c++20']
negative_ir = run('negativo-clang-ir', ['clang++.exe', *cpp, '-S', '-emit-llvm', 'api.cpp', '-o', '-'], negative)
if not re.search(r'x86_regcallcc noundef i32 @"\?t09_z@@[^"]*"\(float %0, float %1, i32', negative_ir):
    raise SystemExit('Clang ya no expande el struct con campo de bits de ancho cero; evidencia: ' + str(root))
run('negativo-compile', ['clang++.exe', *cpp, '-c', 'api.cpp', '-o', 'api.obj'], negative)
run('negativo-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], negative)
output = run('negativo-nsc', [args.nsc, '--dump-project-oop-llvm', 'Fixture.nsproj',
                              '--cpp-clang-root=' + args.clang_root], negative, expected=1)
if 'regcall currently supports only int scalar signatures' not in output:
    raise SystemExit('El struct C++ con campo de bits de ancho cero no fue rechazado como se esperaba; evidencia: ' + str(root))
print('OK: agregados homogeneos y uniones flotantes con regcall comparados con Clang; struct C++ con campo de ancho '
      'cero rechazado; evidencia:', root)
