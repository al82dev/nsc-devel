"""T09-003: enums C por valor con convenciones no cdecl, comparados con Clang 23.1.0.

Clang clasifica un enum por valor como su entero subyacente. Para regcall, sysv_abi,
preserve_most, preserve_all, preserve_none, swiftcall, swiftasynccall e intel_ocl_bicc,
compila funciones C con enums de base int (con negativos, INT_MIN e INT_MAX), unsigned char,
short y unsigned long long, en parametro, retorno e ida y vuelta, mezclados con escalares, y
(regcall, sysv_abi, preserve_*) con un record en parametro y con retorno enum estrecho;
swiftcall, swiftasynccall e intel_ocl_bicc solo admiten firmas escalares. Un control C compilado por Clang y un
consumidor NativeSharp ejecutan las mismas comprobaciones, O0-O3, en biblioteca estatica
y con --dll en DLL; con --precompilado tambien una plantilla consumida con y sin
encabezado. Exige en el IR de NSC la convencion y las extensiones signext/zeroext de cada
parametro y del retorno identicas a la definicion de Clang, y las mismas convenciones en
las llamadas por puntero a funcion de las convenciones que NSC ya proyecta por puntero
(todas salvo swift e intel_ocl_bicc). La plantilla precompilada repite las llamadas directas
y por puntero.
El enum C++ con regcall, el control negativo anterior, se compara ahora con el callee de Clang
(T09-003); la union C solo de float con regcall se compara tambien con Clang y el rechazo se conserva con una
struct C++ con un campo de bits de ancho cero, que NSC no representa al expandirlo.
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
parser.add_argument('--convenciones', default='')
parser.add_argument('--niveles', default='0,1,2,3')
args = parser.parse_args()
root = Path(tempfile.mkdtemp(prefix='nsc-t09-c-enum-convenciones-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)

CC_TOKEN = re.compile(r'\b(x86_\w+cc|win64cc|preserve_\w+cc|swift\w*cc|intel_ocl_bicc|fastcc|coldcc|cc \d+)\b')
POINTER_CONVENTIONS = ('regcall', 'sysv_abi', 'preserve_most', 'preserve_all', 'preserve_none')
RECORD_CONVENTIONS = ('regcall', 'sysv_abi', 'preserve_most', 'preserve_all', 'preserve_none')
CONVENTIONS = tuple(c for c in ('regcall', 'sysv_abi', 'preserve_most', 'preserve_all', 'preserve_none',
                                'swiftcall', 'swiftasynccall', 'intel_ocl_bicc')
                    if not args.convenciones or c in re.split(r'[,+]', args.convenciones))

TYPES = """typedef enum T09Color { T09Red = 1, T09Green = 2, T09Blue = 4 } T09Color;
typedef enum T09Big { T09BigMin = -2147483647 - 1, T09BigNeg = -5, T09BigMax = 2147483647 } T09Big;
typedef enum T09U8 : unsigned char { T09U8a = 1, T09U8b = 200 } T09U8;
typedef enum T09S16 : short { T09S16n = -300, T09S16p = 7 } T09S16;
typedef enum T09U64 : unsigned long long { T09U64a = 1, T09U64b = 0x8000000000000001ULL } T09U64;
typedef struct T09Pair { int a; int b; } T09Pair;
"""
# (name, result, parameters, C body, call check with CALL, needs record)
FUNCTIONS = [
    ('param', 'int', 'T09Color c, int k', 'return (int)c * 10 + k;', 'CALL(T09Blue, 3) != 43', False),
    ('ret', 'T09Color', 'int k', 'return k ? T09Green : T09Red;', 'CALL(1) != T09Green || CALL(0) != T09Red', False),
    ('big', 'T09Big', 'T09Big b', 'return b == T09BigMin ? T09BigMax : b == T09BigMax ? T09BigNeg : T09BigMin;',
     'CALL(T09BigMin) != T09BigMax || CALL(T09BigMax) != T09BigNeg', False),
    ('u8', 'T09U8', 'T09U8 v', 'return v == T09U8a ? T09U8b : T09U8a;', 'CALL(T09U8a) != T09U8b || CALL(T09U8b) != T09U8a', False),
    ('s16', 'T09S16', 'T09S16 v, double d', 'return v == T09S16p && d == 1.5 ? T09S16n : T09S16p;',
     'CALL(T09S16p, 1.5) != T09S16n || CALL(T09S16n, 1.5) != T09S16p', False),
    ('u64', 'T09U64', 'T09U64 v, T09Color c', 'return c == T09Blue ? T09U64b : v;',
     'CALL(T09U64a, T09Blue) != T09U64b || CALL(T09U64a, T09Red) != T09U64a', False),
    ('record', 'int', 'T09Color c, T09Pair p, T09U8 u', 'return (int)c + p.a * p.b + u;',
     'CALL(T09Green, pair, T09U8b) != 217', True),
    ('record_ret', 'T09U8', 'T09Pair p, T09S16 s', 'return p.a == 3 && s == T09S16n ? T09U8b : T09U8a;',
     'CALL(pair, T09S16n) != T09U8b || CALL(pair, T09S16p) != T09U8a', True),
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


def functions_for(convention):
    return [f for f in FUNCTIONS if not f[5] or convention in RECORD_CONVENTIONS]


def header(convention):
    attribute = f'__attribute__(({convention}))'
    text = ['#pragma once', '#if defined(T09_EXPORT)', '#define API __declspec(dllexport)',
            '#elif defined(T09_IMPORT)', '#define API __declspec(dllimport)', '#else', '#define API', '#endif', TYPES]
    for name, result, parameters, _, _, _ in functions_for(convention):
        text.append(f'API {result} {attribute} t09_{name}({parameters});')
        if convention in POINTER_CONVENTIONS:
            text.append(f'typedef {result} ({attribute} *T09Ptr_{name})({parameters});')
            text.append(f'API T09Ptr_{name} t09_get_{name}(void);')
    return '\n'.join(text) + '\n'


def source(convention):
    attribute = f'__attribute__(({convention}))'
    text = ['#include "api.h"', '__declspec(selectany) int _fltused = 0;']
    for name, result, parameters, body, _, _ in functions_for(convention):
        text.append(f'{result} {attribute} t09_{name}({parameters}) {{ {body} }}')
        if convention in POINTER_CONVENTIONS:
            text.append(f'T09Ptr_{name} t09_get_{name}(void) {{ return t09_{name}; }}')
    return '\n'.join(text) + '\n'


def checks(convention, pointers):
    lines, code = [], 10
    for name, _, _, _, check, _ in functions_for(convention):
        lines.append(f'if ({check.replace("CALL", "t09_" + name)}) return {code};'); code += 1
        if pointers and convention in POINTER_CONVENTIONS:
            lines.append(f'T09Ptr_{name} p_{name} = t09_get_{name}(); if ({check.replace("CALL", "p_" + name)}) return {code};')
        code += 1
    return ' '.join(lines)


def control(convention):
    return ('#include "api.h"\n__declspec(selectany) int _fltused = 0;\nint main(void) { T09Pair pair; pair.a = 3; pair.b = 5; '
            + checks(convention, True) + ' return 0; }\n')


def body(convention, pointers=True):
    return 'T09Pair pair; pair.a = 3; pair.b = 5; ' + checks(convention, pointers)


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


def symbol_line(ir, verb, name):
    pattern = re.compile(r'^' + verb + r' .*@(?:__regcall3__)?' + re.escape(name) + r'\(')
    return next((l for l in ir.splitlines() if pattern.search(l)), '')


def project(name, references):
    return ('<Project><PropertyGroup><Name>' + name + '</Name></PropertyGroup><ItemGroup>'
            '<Compile Include="Main.ns" />' + references + '</ItemGroup></Project>')


def c_reference(prefix=''):
    runtime = ' Runtime="' + prefix + 'api.dll"' if args.dll else ''
    return ('<CppLibraryReference Header="' + prefix + 'api.h" Language="C" Standard="c17" Library="'
            + prefix + 'api.lib"' + runtime + ' />')


if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version'], root):
    raise SystemExit('Se requiere Clang 23.1.0')
for convention in CONVENTIONS:
    for opt in [int(level) for level in re.split(r'[,+]', args.niveles)]:
        tag = f'{convention}-O{opt}'
        case = root / tag; case.mkdir()
        (case / 'api.h').write_text(header(convention), encoding='utf-8')
        (case / 'api.c').write_text(source(convention), encoding='utf-8')
        (case / 'Control.c').write_text(control(convention), encoding='utf-8')
        flags = ['-target', 'x86_64-pc-windows-msvc', '-std=c17', '-O' + str(opt)]
        run(f'{tag}-api', ['clang.exe', *flags, *(['-DT09_EXPORT'] if args.dll else []), '-c', 'api.c',
                           '-o', 'api.obj'], case)
        clang_ir = run(f'{tag}-api-ir', ['clang.exe', *flags, '-O0', '-S', '-emit-llvm', 'api.c', '-o', '-'], case)
        expected = {}
        for name, *_ in functions_for(convention):
            signature = abi_signature(symbol_line(clang_ir, 'define', 't09_' + name))
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
        run(f'{tag}-control-obj', ['clang.exe', *flags, '-c', 'Control.c', '-o', 'control.obj'], case)
        run(f'{tag}-control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console',
                                    '/nodefaultlib', '/out:control.exe', 'control.obj', 'api.lib'], case)
        run(f'{tag}-control-run', [str(case / 'control.exe')], case)
        (case / 'Main.ns').write_text('int Main() { ' + body(convention) + ' return 0; }\n', encoding='utf-8')
        (case / 'Fixture.nsproj').write_text(project('Fixture', c_reference()), encoding='utf-8')
        common = ['Fixture.nsproj', '--cpp-clang-root=' + args.clang_root]
        ir = run(f'{tag}-ir', [args.nsc, '--dump-project-oop-llvm', *common], case)
        for name, signature in expected.items():
            observed = abi_signature(symbol_line(ir, 'declare', 't09_' + name))
            if observed != signature:
                raise SystemExit(f'{tag}: t09_{name}: NSC {observed!r} frente a Clang {signature!r}; evidencia: {root}')
            if not re.search(r'call ' + re.escape(signature[0]) + r' [^@\n]*@(?:__regcall3__)?t09_' + name + r'\(', ir):
                raise SystemExit(f'{tag}: la llamada directa a t09_{name} no usa {signature[0]}; evidencia: {root}')
        if convention in POINTER_CONVENTIONS:
            user = ir.split('define internal i1 @__nativesharp_rtti')[0]
            indirect = re.findall(r'call ' + re.escape(next(iter(expected.values()))[0]) + r' [^@\n]*%[\w.]+\(', user)
            if len(indirect) < len(expected):
                raise SystemExit(f'{tag}: {len(indirect)} llamadas por puntero con la convencion, se esperaban '
                                 f'{len(expected)}; evidencia: {root}')
        run(f'{tag}-build', [args.nsc, '--emit-project-oop-exe', *common, '--no-artifact-cache', '-O' + str(opt),
                             '-o', 'native.exe'], case)
        run(f'{tag}-native-run', [str(case / 'native.exe')], case)
        print(f'{tag}: control C y NativeSharp ' + ('directo y por puntero' if convention in POINTER_CONVENTIONS
              else 'directo') + ' pasaron', flush=True)
        if args.precompilado:
            producer = case / 'producer'; producer.mkdir()
            for name in ('api.h', 'api.lib') + (('api.dll',) if args.dll else ()):
                shutil.copyfile(case / name, producer / name)
            (producer / 'Producer.ns').write_text('namespace T09 { public class Bridge { public static int Run<T>(T unused) { '
                + body(convention) + ' return 0; } } }\n', encoding='utf-8')
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
                (consumer / 'Main.ns').write_text('int Main() { return T09.Bridge.Run<int>(0); }\n', encoding='utf-8')
                (consumer / 'Consumer.nsproj').write_text(project('T09Consumer', references), encoding='utf-8')
                consumer_ir = run(f'{tag}-{visibility}-ir', [args.nsc, '--dump-project-oop-llvm', 'Consumer.nsproj',
                    '--cpp-clang-root=' + args.clang_root], consumer)
                for name, signature in expected.items():
                    observed = abi_signature(symbol_line(consumer_ir, 'declare', 't09_' + name))
                    if observed != signature:
                        raise SystemExit(f'{tag} {visibility}: t09_{name}: NSC {observed!r} frente a Clang '
                                         f'{signature!r}; evidencia: {root}')
                if convention in POINTER_CONVENTIONS:
                    cc = next(iter(expected.values()))[0]
                    indirect = re.findall(r'call ' + re.escape(cc) + r' [^@\n]*%[\w.]+\(',
                                          consumer_ir.split('define internal i1 @__nativesharp_rtti')[0])
                    if len(indirect) < len(expected):
                        raise SystemExit(f'{tag} {visibility}: {len(indirect)} llamadas por puntero con {cc}; '
                                         f'evidencia: {root}')
                run(f'{tag}-{visibility}-build', [args.nsc, '--emit-project-oop-exe', 'Consumer.nsproj',
                    '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O' + str(opt), '-o',
                    'consumer.exe'], consumer)
                if args.dll: shutil.copyfile(producer / 'api.dll', consumer / 'api.dll')
                run(f'{tag}-{visibility}-run', [str(consumer / 'consumer.exe')], consumer)
                print(f'{tag}: plantilla precompilada {visibility} paso', flush=True)

# Former negative: a C++ enum by value with regcall, now compared with the callee compiled by Clang.
negative = root / 'negativo-cpp-enum'; negative.mkdir()
(negative / 'api.h').write_text('#pragma once\nnamespace T09N { enum class Mode : int { A = 1, B = 3 }; '
    'int __attribute__((regcall)) t09_mode(Mode m, int k); }\n', encoding='utf-8')
(negative / 'api.cpp').write_text('#include "api.h"\nnamespace T09N { int __attribute__((regcall)) '
    't09_mode(Mode m, int k) { return (int)m * 10 + k; } }\n', encoding='utf-8')
(negative / 'Main.ns').write_text('int Main() { return T09N.t09_mode(T09N.Mode.B, 4) == 34 ? 0 : 1; }\n', encoding='utf-8')
(negative / 'Fixture.nsproj').write_text('<Project><PropertyGroup><Name>Fixture</Name></PropertyGroup><ItemGroup>'
    '<Compile Include="Main.ns" /><CppLibraryReference Header="api.h" Language="C++" Standard="c++20" '
    'Library="api.lib" /></ItemGroup></Project>', encoding='utf-8')
run('negativo-compile', ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-c', 'api.cpp',
                         '-o', 'api.obj'], negative)
run('negativo-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], negative)

# EN: T09-003. The former negative input, a C++ enum class by value with regcall, is now projected through Clang's
#     plan: the callee compiled by Clang returns the expected value and NSC calls it with x86_regcallcc.
# ES: T09-003. La entrada negativa anterior, un enum class C++ por valor con regcall, ya se proyecta por el plan de
#     Clang: el callee compilado por Clang devuelve el valor esperado y NSC lo llama con x86_regcallcc.
enumcpp_ir = subprocess.run([args.nsc, '--dump-project-oop-llvm', 'Fixture.nsproj', '--cpp-clang-root=' + args.clang_root],
                             cwd=negative, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
(root / 'enumcpp-ir.log').write_text('EXIT: ' + str(enumcpp_ir.returncode) + '\n' + enumcpp_ir.stdout, encoding='utf-8')
if enumcpp_ir.returncode or not re.search(r'(?:call|invoke) x86_regcallcc i32 @"\?t09_mode@', enumcpp_ir.stdout):
    raise SystemExit('El enum C++ con regcall no se llama con x86_regcallcc; evidencia: ' + str(root))
for enumcpp_opt in range(4):
    enumcpp_exe = str(negative / ('native-enum-cpp.exe'.replace('.exe', '-O' + str(enumcpp_opt) + '.exe')))
    enumcpp_build = subprocess.run([args.nsc, '--emit-project-oop-exe', 'Fixture.nsproj', '--cpp-clang-root=' + args.clang_root,
                                     '--no-artifact-cache', '-O' + str(enumcpp_opt), '-o', enumcpp_exe],
                                    cwd=negative, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
    (root / ('enumcpp-build-O' + str(enumcpp_opt) + '.log')).write_text(enumcpp_build.stdout, encoding='utf-8')
    if enumcpp_build.returncode or subprocess.run([enumcpp_exe], cwd=negative, env=env).returncode:
        raise SystemExit('El enum C++ con regcall no dio el resultado de Clang en O' + str(enumcpp_opt) + '; evidencia: ' + str(root))

# EN: T09-003. A C union of floats by value with regcall, the former rejection check, is now compared with
#     Clang: Clang expands it as a float, its first largest field. The rejection check is kept with a C++
#     struct that has a zero-width bitfield between two floats: Clang skips the bitfield and expands two
#     floats, and NSC does not represent bitfields in an expansion.
# ES: T09-003. La union C solo de float por valor con regcall, antes control negativo, se compara ahora con
#     Clang, que la expande como float, su primer campo de mayor tamano. El rechazo se conserva con un struct
#     C++ con un campo de bits de ancho cero entre dos float: Clang omite el campo y expande dos float, y NSC
#     no representa campos de bits en una expansion.
def t09_regcall_union(label, declaration):
    import re as _re
    case = root / label; case.mkdir()
    (case / 'api.h').write_text('#pragma once\n' + declaration + '\n'
        'int __attribute__((regcall)) t09_uf(T09UF u, int k);\n', encoding='utf-8')
    (case / 'api.c').write_text('#include "api.h"\nint __attribute__((regcall)) t09_uf(T09UF u, int k) '
        '{ return (int)u.a + k; }\n', encoding='utf-8')
    (case / 'Control.c').write_text('__declspec(selectany) int _fltused = 0;\n#include "api.h"\n'
        'int main(void) { T09UF u; u.a = 3; return t09_uf(u, 4) == 7 ? 0 : 1; }\n', encoding='utf-8')
    (case / 'Main.ns').write_text('int Main() { T09UF u; u.a = 3; return t09_uf(u, 4) == 7 ? 0 : 1; }\n', encoding='utf-8')
    (case / 'Union.nsproj').write_text('<Project><PropertyGroup><Name>Union</Name></PropertyGroup><ItemGroup>'
        '<Compile Include="Main.ns" /><CppLibraryReference Header="api.h" Language="C" Standard="c17" '
        'Library="api.lib" /></ItemGroup></Project>', encoding='utf-8')
    def step(name, command, expected=0):
        result = subprocess.run(command, cwd=case, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                text=True, errors='replace')
        (case / (name + '.log')).write_text('EXIT: ' + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
        if (expected == 0) != (result.returncode == 0):
            raise SystemExit(label + ': ' + name + ' termino con ' + str(result.returncode) + '; evidencia: ' + str(root))
        return result.stdout
    target = ['-target', 'x86_64-pc-windows-msvc', '-std=c17']
    clang_ir = step('clang-ir', ['clang.exe', *target, '-S', '-emit-llvm', 'api.c', '-o', '-'])
    if not _re.search(r'define dso_local x86_regcallcc i32 @__regcall3__t09_uf\(float ', clang_ir):
        raise SystemExit(label + ': Clang no expande la union como float; evidencia: ' + str(root))
    for opt in range(4):
        flags = [*target, '-O' + str(opt)]
        step('clang-O' + str(opt), ['clang.exe', *flags, '-c', 'api.c', '-o', 'api.obj'])
        step('lib-O' + str(opt), ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'])
        step('control-obj-O' + str(opt), ['clang.exe', *flags, '-c', 'Control.c', '-o', 'control.obj'])
        step('control-O' + str(opt), ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console', '/nodefaultlib',
                                      '/out:control.exe', 'control.obj', 'api.lib'])
        step('control-run-O' + str(opt), [str(case / 'control.exe')])
        ir = step('nsc-ir-O' + str(opt), [args.nsc, '--dump-project-oop-llvm', 'Union.nsproj', '--cpp-clang-root=' + args.clang_root])
        if not _re.search(r'declare x86_regcallcc i32 @__regcall3__t09_uf\(float[^,]*, i32', ir) or \
                not _re.search(r'call x86_regcallcc i32 @__regcall3__t09_uf\(float ', ir):
            raise SystemExit(label + ': el IR de NSC no pasa la union como float con x86_regcallcc; evidencia: ' + str(root))
        step('nsc-O' + str(opt), [args.nsc, '--emit-project-oop-exe', 'Union.nsproj', '--cpp-clang-root=' + args.clang_root,
                                  '--no-artifact-cache', '-O' + str(opt), '-o', 'native.exe'])
        step('native-run-O' + str(opt), [str(case / 'native.exe')])


def t09_regcall_zero_width_rejected():
    import re as _re
    case = root / 'struct-cpp-campo-ancho-cero'; case.mkdir()
    (case / 'api.h').write_text('#pragma once\nstruct T09Z { float a; int : 0; float b; };\n'
        'int __attribute__((regcall)) t09_z(T09Z v, int k);\n', encoding='utf-8')
    (case / 'api.cpp').write_text('#include "api.h"\nint __attribute__((regcall)) t09_z(T09Z v, int k) '
        '{ return (int)(v.a + v.b) + k; }\n', encoding='utf-8')
    (case / 'Main.ns').write_text('int Main() { T09Z v; v.a = 3; v.b = 0; return t09_z(v, 4) == 7 ? 0 : 1; }\n',
                                  encoding='utf-8')
    (case / 'Rejected.nsproj').write_text('<Project><PropertyGroup><Name>Rejected</Name></PropertyGroup><ItemGroup>'
        '<Compile Include="Main.ns" /><CppLibraryReference Header="api.h" Language="C++" Standard="c++20" '
        'Library="api.lib" /></ItemGroup></Project>', encoding='utf-8')
    def step(name, command, expected=0):
        result = subprocess.run(command, cwd=case, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                text=True, errors='replace')
        (case / (name + '.log')).write_text('EXIT: ' + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
        if (expected == 0) != (result.returncode == 0):
            raise SystemExit('struct-cpp-campo-ancho-cero: ' + name + ' termino con ' + str(result.returncode)
                             + '; evidencia: ' + str(root))
        return result.stdout
    target = ['-target', 'x86_64-pc-windows-msvc', '-std=c++20']
    clang_ir = step('clang-ir', ['clang++.exe', *target, '-S', '-emit-llvm', 'api.cpp', '-o', '-'])
    if not _re.search(r'x86_regcallcc noundef i32 @"\?t09_z@@[^"]*"\(float %0, float %1, i32', clang_ir):
        raise SystemExit('Clang ya no expande el struct con campo de bits de ancho cero; evidencia: ' + str(root))
    step('clang', ['clang++.exe', *target, '-c', 'api.cpp', '-o', 'api.obj'])
    step('lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'])
    output = step('nsc', [args.nsc, '--dump-project-oop-llvm', 'Rejected.nsproj', '--cpp-clang-root=' + args.clang_root], 1)
    if 'regcall currently supports only int scalar signatures' not in output:
        raise SystemExit('El struct C++ con campo de bits de ancho cero no fue rechazado como se esperaba; evidencia: '
                         + str(root))


t09_regcall_union('union-float', 'typedef union T09UF { float a; float b; } T09UF;')
t09_regcall_zero_width_rejected()
print('OK: enums C y C++ por valor con convenciones no cdecl comparados con Clang; union solo float regcall comparada con Clang; struct C++ con campo de ancho cero rechazado; '
      'evidencia:', root)
