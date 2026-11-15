"""T09-003: unions C por valor con convenciones no cdecl, comparadas con Clang 23.1.0.

Para regcall, sysv_abi, preserve_most, preserve_all y preserve_none compila funciones C con
unions por valor de 4, 8, 12 y 24 bytes (enteras, mixtas y solo float) en parametro, en
retorno y en ida y vuelta, mezcladas con escalares, y punteros a funcion con las mismas
firmas. La union solo float tambien entra en regcall, que Clang expande como float (T09-003).
Un control C compilado por Clang y un consumidor NativeSharp
ejecutan las mismas comprobaciones, O0-O3, en biblioteca estatica y con --dll en DLL; con
--precompilado tambien una plantilla consumida con y sin encabezado, con llamadas directas
y por puntero. Exige en el IR de NSC la convencion de Clang en cada declaracion y llamada
directa y en todas las llamadas indirectas, y que la union original no cambie.
La union solo float con regcall era el control negativo; ahora se compara con Clang y el
rechazo se conserva con un struct C++ con un campo de bits de ancho cero entre dos float, que
Clang omite al expandir y NSC no representa.
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
root = Path(tempfile.mkdtemp(prefix='nsc-t09-c-union-convenciones-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)

CC_TOKEN = re.compile(r'\b(x86_\w+cc|win64cc|preserve_\w+cc|swift\w*cc|intel_ocl_bicc|fastcc|coldcc|cc \d+)\b')
POINTER_CONVENTIONS = ('regcall', 'sysv_abi', 'preserve_most', 'preserve_all', 'preserve_none')
RECORD_CONVENTIONS = POINTER_CONVENTIONS
CONVENTIONS = tuple(c for c in POINTER_CONVENTIONS
                    if not args.convenciones or c in re.split(r'[,+]', args.convenciones))

TYPES = """typedef union T09U4 { int i; float f; } T09U4;
typedef union T09U8 { long long i; double d; } T09U8;
typedef union T09U12 { int v[3]; float f; } T09U12;
typedef union T09UF { float f; float g; } T09UF;
typedef union T09U24 { long long v[3]; double d; } T09U24;
typedef struct T09Pair { int a; int b; } T09Pair;
"""
# (name, result, parameters, C body, call check with CALL, conventions that skip it)
FUNCTIONS = [
    ('u4', 'int', 'T09U4 u, int k', 'return u.i + k;', 'CALL(x4, 4) != 7 || x4.i != 3', ()),
    ('u8_ret', 'T09U8', 'T09U8 u', 'u.i += 5; return u;', 'T09U8 RES = CALL(x8); | RES.i != 8 || x8.i != 3', ()),
    ('u12', 'int', 'T09U12 u, double d', 'return u.v[0] + u.v[2] + (int)d;', 'CALL(x12, 2.0) != 6 || x12.v[2] != 3', ()),
    ('uf', 'float', 'T09UF u, float x', 'return u.f + x;', 'CALL(xf, 2.0f) != 3.5f || xf.f != 1.5f', ()),
    ('u24', 'long long', 'T09U24 u, int k', 'return u.v[0] + u.v[2] + k;', 'CALL(x24, 4) != (long)8 || x24.v[2] != (long)3', ()),
    ('u24_ret', 'T09U24', 'T09U24 u, T09Pair p', 'u.v[2] += p.a; return u;',
     'T09U24 RES = CALL(x24, pair); | RES.v[2] != (long)6 || x24.v[2] != (long)3', ()),
]
SETUP = ('T09Pair pair; pair.a = 3; pair.b = 5; T09U4 x4; x4.i = 3; T09U8 x8; x8.i = 3; '
         'T09U12 x12; x12.v[0] = 1; x12.v[1] = 2; x12.v[2] = 3; T09UF xf; xf.f = 1.5f; '
         'T09U24 x24; x24.v[0] = 1; x24.v[1] = 2; x24.v[2] = 3; ')
C_SETUP = ('T09Pair pair = {3, 5}; T09U4 x4; x4.i = 3; T09U8 x8; x8.i = 3; '
           'T09U12 x12 = {{1, 2, 3}}; T09UF xf; xf.f = 1.5f; T09U24 x24 = {{1, 2, 3}}; ')


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
    return [f for f in FUNCTIONS if convention not in f[5]]


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
        # 'statement | condition' checks a record result through a local named after the call site.
        prefix, condition = check.split(' | ') if ' | ' in check else ('', check)
        def site(callee, local):
            return (prefix + ' ' if prefix else '').replace('CALL', callee).replace('RES', local) + \
                f'if ({condition.replace("CALL", callee).replace("RES", local)}) return {{}};'
        lines.append(site('t09_' + name, 'r_' + name).format(code)); code += 1
        if pointers and convention in POINTER_CONVENTIONS:
            lines.append(f'T09Ptr_{name} p_{name} = t09_get_{name}(); ' + site('p_' + name, 'q_' + name).format(code))
        code += 1
    return ' '.join(lines)


def control(convention):
    return ('#include "api.h"\n__declspec(selectany) int _fltused = 0;\n'
            'void *memcpy(void *d, const void *s, unsigned long long n)\n'
            '{ volatile unsigned char *o = d; const volatile unsigned char *i = s;\n'
            '  for (unsigned long long k = 0; k < n; ++k) o[k] = i[k]; return d; }\n'
            'int main(void) { ' + C_SETUP + checks(convention, True)
            + ' return 0; }\n')


def body(convention, pointers=True):
    return SETUP + checks(convention, pointers)


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

# Negative: Clang skips a zero-width bitfield between two floats of a C++ struct and expands two floats with
# regcall; NSC does not represent bitfields in an expansion, so the signature is still rejected.
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
print('OK: unions C por valor con convenciones no cdecl comparadas con Clang; struct C++ con campo de ancho cero rechazado; '
      'evidencia:', root)
