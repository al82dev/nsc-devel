"""T09-003: records C con atributos de alineacion en punteros sysv_abi y regcall desde plantilla precompilada.

El catalogo no guarda los atributos de alineacion. El consumidor de una plantilla precompilada reconstruye
con Clang 23.1.0 los records C de la firma (replay) y les devuelve la menor alineacion que da cada
desplazamiento observado y la alineacion del record; despues compara todo el layout reconstruido con el
catalogo. Para sysv_abi y regcall y para campos _Alignas (intermedio, primero, final, char y double), un
record con __attribute__((aligned)) o __declspec(align) y una union con un campo _Alignas, compara un
control C compilado por Clang con la plantilla consumida con y sin encabezado, O0-O3, en biblioteca
estatica y con --dll en DLL, y exige la convencion de Clang en la llamada indirecta del consumidor.
Ademas, para una llamada directa sysv_abi variadica que recibe el record como argumento fijo y como
argumento variable, exige que los argumentos fisicos del IR de NSC coincidan con los de la llamada de
Clang. Los controles negativos conservan el rechazo de un campo __unaligned, que el replay no
reconstruye, y de un record con alineacion y campos de bits, que el productor deja fuera, en el
puntero y en la llamada variadica.
T19-030: esos dos negativos eran obsoletos y pasan a casos positivos comparados con Clang, junto con
#pragma pack(1) y (2) (tambien con campos de bits), un campo packed, un record deprecated y uno
may_alias, un campo unused, un campo annotate y un record alineado cuyo primer campo es de bits. Queda
un rechazo genuino que Clang comparte: _Alignas menor que la alineacion natural del campo, que Clang no
compila y NSC rechaza con el diagnostico de Clang.
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
args = parser.parse_args()
root = Path(tempfile.mkdtemp(prefix='nsc-t09-alineacion-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)

NESTED = 'typedef struct T09In { int a; int b; } T09In;\n'
# (name, record fields, C initializer, NativeSharp initialization, C body using r and k, expected result)
CASES = [
    ('alignas-campo', 'int x; _Alignas(16) int z;', '{3, 7}', 'r.x = 3; r.z = 7;', 'return r.x + r.z * k;', 31),
    ('alignas-char', 'char c; _Alignas(8) char d; int x;', '{1, 2, 3}', 'r.x = 3;', 'return r.x * k;', 12),
    ('alignas-primero', '_Alignas(16) int x; int y;', '{3, 5}', 'r.x = 3; r.y = 5;', 'return r.x + r.y * k;', 23),
    ('alignas-final', 'int a; int b; _Alignas(8) int c;', '{1, 2, 3}', 'r.a = 1; r.c = 3;', 'return r.a + r.c * k;', 13),
    ('alignas-double', 'int x; _Alignas(32) double d;', '{3, 2.5}', 'r.x = 3; r.d = 2.5;',
     'return r.x + (int)(r.d * k);', 13),
    ('aligned-record', 'ALIGNED16 int x; int y;', '{3, 5}', 'r.x = 3; r.y = 5;', 'return r.x + r.y * k;', 23),
    ('declspec-record', 'DECLSPEC16 int x; int y;', '{3, 5}', 'r.x = 3; r.y = 5;', 'return r.x + r.y * k;', 23),
    ('union-alignas', 'UNION _Alignas(16) int x; long long y;', '{5}', 'r.x = 5;', 'return r.x * k;', 20),
    # T19-030: former negatives, now compared with Clang. / Negativos anteriores, ahora comparados con Clang.
    ('unaligned', 'int x; int y; __unaligned int z;', '{3, 5, 7}', 'r.x = 3; r.y = 5; r.z = 7;',
     'return r.x + r.z * k;', 31),
    ('alignas-bits', 'int x : 4; _Alignas(16) int z;', '{3, 7}', 'r.x = 3; r.z = 7;', 'return r.x + r.z * k;', 31),
    ('pack1', 'PACK1 char c; int x; short s;', '{1, 3, 5}', 'r.x = 3; r.s = 5;', 'return r.x + r.s * k;', 23),
    ('pack2', 'PACK2 char c; int x; double d;', '{1, 3, 2.5}', 'r.x = 3; r.d = 2.5;',
     'return r.x + (int)(r.d * k);', 13),
    ('deprecated', 'DEPRECATED int x; int y;', '{3, 5}', 'r.x = 3; r.y = 5;', 'return r.x + r.y * k;', 23),
    ('unused-annotate', 'int x; __attribute__((unused)) int y; __attribute__((annotate("t19"))) int z;',
     '{3, 5, 7}', 'r.x = 3; r.z = 7;', 'return r.x + r.z * k;', 31),
    ('aligned-bits-primero', 'ALIGNED16 int a : 3; int b : 5; char c;', '{3, 1, 5}', 'r.a = 3; r.c = 5;',
     'return r.a + r.c * k;', 23),
    ('campo-packed', 'char c; __attribute__((packed)) int x; short s;', '{1, 3, 5}', 'r.x = 3; r.s = 5;',
     'return r.x + r.s * k;', 23),
    ('pack2-bits', 'PACK2 char c; int a : 3; int b : 5; double d;', '{1, 3, 1, 2.5}', 'r.a = 3; r.d = 2.5;',
     'return r.a + (int)(r.d * k);', 13),
    ('may-alias', 'MAYALIAS int x; int y;', '{3, 5}', 'r.x = 3; r.y = 5;', 'return r.x + r.y * k;', 23),
]
# EN: T19-030. A genuine rejection that Clang shares: an alignment below the natural one does not compile.
# ES: T19-030. Un rechazo genuino que Clang comparte: una alineacion menor que la natural no compila.
NEGATIVES = [
    ('alignas-menor', 'int x; _Alignas(2) int z;', '{3, 7}', 'r.x = 3; r.z = 7;', 'return r.x + r.z * k;', 31),
]
RECORD_PREFIXES = {'ALIGNED16 ': 'typedef struct __attribute__((aligned(16))) T09R { ',
                   'DECLSPEC16 ': 'typedef struct __declspec(align(16)) T09R { ',
                   'UNION ': 'typedef union T09R { ',
                   'PACK1 ': '#pragma pack(push, 1)\ntypedef struct T09R { ',
                   'PACK2 ': '#pragma pack(push, 2)\ntypedef struct T09R { ',
                   'DEPRECATED ': 'typedef struct __attribute__((deprecated)) T09R { ',
                   'MAYALIAS ': 'typedef struct __attribute__((may_alias)) T09R { '}
CC = {'sysv_abi': 'x86_64_sysvcc', 'regcall': 'x86_regcallcc'}


def run(label, command, cwd, expected=0):
    result = subprocess.run(command, cwd=cwd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, errors='replace')
    log = root / (label + '.log')
    log.write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' + result.stdout,
                   encoding='utf-8')
    if expected is not None and ((expected == 0 and result.returncode) or (expected != 0 and result.returncode == 0)):
        print(result.stdout[-3000:], flush=True)
        raise SystemExit(label + ': codigo ' + str(result.returncode) + ' inesperado; evidencia: ' + str(log))
    return result


def record_text(fields):
    for prefix, text in RECORD_PREFIXES.items():
        if fields.startswith(prefix):
            return text + fields[len(prefix):]
    return 'typedef struct T09R { ' + fields


def header(convention, fields):
    return ('#pragma once\n#if defined(T09_EXPORT)\n#define API __declspec(dllexport)\n#elif defined(T09_IMPORT)\n'
            '#define API __declspec(dllimport)\n#else\n#define API\n#endif\n' + NESTED +
            record_text(fields) + ' } T09R;\n' + ('#pragma pack(pop)\n' if fields.startswith('PACK') else '') +
            f'typedef int (__attribute__(({convention})) *T09Ptr)(T09R r, int k);\nAPI T09Ptr t09_get(void);\n')


def c_reference(prefix=''):
    runtime = ' Runtime="' + prefix + 'api.dll"' if args.dll else ''
    return ('<CppLibraryReference Header="' + prefix + 'api.h" Language="C" Standard="c17" Library="'
            + prefix + 'api.lib"' + runtime + ' />')


def project(name, references, library=False):
    output = '<OutputType>Library</OutputType>' if library else ''
    source = 'Producer.ns' if library else 'Main.ns'
    return ('<Project><PropertyGroup><Name>' + name + '</Name>' + output + '</PropertyGroup><ItemGroup>'
            '<Compile Include="' + source + '" />' + references + '</ItemGroup></Project>')


def prepare(convention, item, opt):
    name, fields, c_init, ns_init, body, expected = item
    tag = f'{convention}-{name}-O{opt}'
    case = root / tag
    producer = case / 'producer'
    producer.mkdir(parents=True)
    (producer / 'api.h').write_text(header(convention, fields), encoding='utf-8')
    (producer / 'api.c').write_text('#include "api.h"\n__declspec(selectany) int _fltused = 0;\n'
        f'static int __attribute__(({convention})) t09_f(T09R r, int k) {{ {body} }}\n'
        'T09Ptr t09_get(void) { return t09_f; }\n', encoding='utf-8')
    (producer / 'Control.c').write_text('#include "api.h"\n__declspec(selectany) int _fltused = 0;\n'
        'void *memcpy(void *d, const void *s, unsigned long long n)\n'
        '{ volatile unsigned char *o = d; const volatile unsigned char *i = s;\n'
        '  for (unsigned long long q = 0; q < n; ++q) o[q] = i[q]; return d; }\n'
        'void *memset(void *d, int v, unsigned long long n)\n'
        '{ volatile unsigned char *o = d; for (unsigned long long q = 0; q < n; ++q) o[q] = (unsigned char)v; return d; }\n'
        f'int main(void) {{ T09R r = {c_init}; return t09_get()(r, 4); }}\n', encoding='utf-8')
    flags = ['-target', 'x86_64-pc-windows-msvc', '-std=c17', '-O' + str(opt)]
    run(tag + '-api', ['clang.exe', *flags, *(['-DT09_EXPORT'] if args.dll else []), '-c', 'api.c', '-o', 'api.obj'],
        producer)
    if args.dll:
        run(tag + '-lib', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib', '/out:api.dll',
                           '/implib:api.lib', 'api.obj'], producer)
        path = producer / 'api.h'
        path.write_text('#define T09_IMPORT 1\n' + path.read_text(encoding='utf-8'), encoding='utf-8')
    else:
        run(tag + '-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], producer)
    run(tag + '-control-obj', ['clang.exe', *flags, '-c', 'Control.c', '-o', 'control.obj'], producer)
    run(tag + '-control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console', '/nodefaultlib',
                                '/out:control.exe', 'control.obj', 'api.lib'], producer)
    control = run(tag + '-control-run', [str(producer / 'control.exe')], producer, expected=None).returncode
    if control != expected:
        raise SystemExit(f'{tag}: el control C devolvio {control}, se esperaba {expected}; evidencia: {root}')
    (producer / 'Producer.ns').write_text('namespace T09 { public class Bridge { public static int Run<T>(T unused) { '
        'T09R r; ' + ns_init + ' T09Ptr p = t09_get(); return p(r, 4); } } }\n', encoding='utf-8')
    (producer / 'Producer.nsproj').write_text(project('T09Producer', c_reference(), True), encoding='utf-8')
    run(tag + '-producer', [args.nsc, '--emit-project-oop-lib', 'Producer.nsproj', '--cpp-clang-root=' + args.clang_root,
                            '--no-artifact-cache', '-O' + str(opt), '-o', 'Producer.lib'], producer)
    return tag, case, producer, expected


def prepare_variadic(item, opt):
    name, fields, c_init, ns_init, body, expected = item
    tag = f'sysv_abi-variadica-{name}-O{opt}'
    case = root / tag
    producer = case / 'producer'
    producer.mkdir(parents=True)
    text = header('sysv_abi', fields).replace(
        'API T09Ptr t09_get(void);', 'API int __attribute__((sysv_abi)) t09_v(T09R r, int k, ...);')
    (producer / 'api.h').write_text(text, encoding='utf-8')
    (producer / 'api.c').write_text('#include "api.h"\n__declspec(selectany) int _fltused = 0;\n'
        f'int __attribute__((sysv_abi)) t09_v(T09R r, int k, ...) {{ {body} }}\n', encoding='utf-8')
    (producer / 'Control.c').write_text('#include "api.h"\n__declspec(selectany) int _fltused = 0;\n'
        'void *memcpy(void *d, const void *s, unsigned long long n)\n'
        '{ volatile unsigned char *o = d; const volatile unsigned char *i = s;\n'
        '  for (unsigned long long q = 0; q < n; ++q) o[q] = i[q]; return d; }\n'
        'void *memset(void *d, int v, unsigned long long n)\n'
        '{ volatile unsigned char *o = d; for (unsigned long long q = 0; q < n; ++q) o[q] = (unsigned char)v; return d; }\n'
        f'int main(void) {{ T09R r = {c_init}; return t09_v(r, 4, 9, r); }}\n', encoding='utf-8')
    flags = ['-target', 'x86_64-pc-windows-msvc', '-std=c17', '-O' + str(opt)]
    run(tag + '-api', ['clang.exe', *flags, *(['-DT09_EXPORT'] if args.dll else []), '-c', 'api.c', '-o', 'api.obj'],
        producer)
    if args.dll:
        run(tag + '-lib', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib', '/out:api.dll',
                           '/implib:api.lib', 'api.obj'], producer)
        path = producer / 'api.h'
        path.write_text('#define T09_IMPORT 1\n' + path.read_text(encoding='utf-8'), encoding='utf-8')
    else:
        run(tag + '-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], producer)
    run(tag + '-control-ir', ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c17', '-O0', '-S', '-emit-llvm',
                              'Control.c', '-o', 'control.ll'], producer)
    run(tag + '-control-obj', ['clang.exe', *flags, '-c', 'Control.c', '-o', 'control.obj'], producer)
    run(tag + '-control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console', '/nodefaultlib',
                                '/out:control.exe', 'control.obj', 'api.lib'], producer)
    control = run(tag + '-control-run', [str(producer / 'control.exe')], producer, expected=None).returncode
    if control != expected:
        raise SystemExit(f'{tag}: el control C devolvio {control}, se esperaba {expected}; evidencia: {root}')
    (producer / 'Producer.ns').write_text('namespace T09 { public class Bridge { public static int Run<T>(T unused) { '
        'T09R r; ' + ns_init + ' return t09_v(r, 4, 9, r); } } }\n', encoding='utf-8')
    (producer / 'Producer.nsproj').write_text(project('T09Producer', c_reference(), True), encoding='utf-8')
    run(tag + '-producer', [args.nsc, '--emit-project-oop-lib', 'Producer.nsproj', '--cpp-clang-root=' + args.clang_root,
                            '--no-artifact-cache', '-O' + str(opt), '-o', 'Producer.lib'], producer)
    return tag, case, producer, expected


def physical_arguments(ir, tag):
    """Physical argument shapes of the only sysv_abi call to t09_v: type, extension and byval, without values."""
    calls = re.findall(r'call x86_64_sysvcc [^@\n]*@t09_v\(([^\n]*)\)', ir)
    if len(calls) != 1:
        raise SystemExit(f'{tag}: se esperaba una llamada x86_64_sysvcc a t09_v, hay {len(calls)}; evidencia: {root}')
    shapes = []
    for argument in calls[0].split(', '):
        # The byval type is named differently by Clang (%struct.T09R) and NSC ([N x i8]); only its presence counts.
        argument = re.sub(r'byval\([^()]*\)', 'byval', argument)
        words = [w for w in argument.split() if w not in ('noundef', 'nonnull')]
        kept = [words[0]] + words[1:-1]
        if 'align' in kept:
            at = kept.index('align')
            kept[at:at + 2] = ['align ' + kept[at + 1]]
        shapes.append(' '.join(kept))
    return shapes


def consume(tag, case, producer, visibility, opt, expect_success=True):
    consumer = case / visibility
    consumer.mkdir()
    references = '<LibraryReference Include="../producer/Producer.lib" />'
    if visibility == 'con-encabezado':
        references += c_reference('../producer/')
    elif (producer / 'api.h').exists():
        (producer / 'api.h').rename(producer / 'api.h.oculto')
    (consumer / 'Main.ns').write_text('int Main() { return T09.Bridge.Run<int>(0); }\n', encoding='utf-8')
    (consumer / 'Consumer.nsproj').write_text(project('T09Consumer', references), encoding='utf-8')
    common = ['Consumer.nsproj', '--cpp-clang-root=' + args.clang_root]
    ir = run(f'{tag}-{visibility}-ir', [args.nsc, '--dump-project-oop-llvm', *common], consumer,
             expected=0 if expect_success else 1)
    return consumer, ir.stdout


if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version'], root).stdout:
    raise SystemExit('Se requiere Clang 23.1.0')
for convention in ('sysv_abi', 'regcall'):
    for item in CASES:
        for opt in range(4):
            tag, case, producer, expected = prepare(convention, item, opt)
            for visibility in ('con-encabezado', 'sin-encabezado'):
                consumer, ir = consume(tag, case, producer, visibility, opt)
                user = ir.split('define internal i1 @__nativesharp_rtti')[0]
                if not re.search(r'call ' + CC[convention] + r' [^@\n]*%[\w.]+\(', user):
                    raise SystemExit(f'{tag} {visibility}: falta la llamada indirecta {CC[convention]}; evidencia: {root}')
                run(f'{tag}-{visibility}-build', [args.nsc, '--emit-project-oop-exe', 'Consumer.nsproj',
                    '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O' + str(opt), '-o', 'consumer.exe'],
                    consumer)
                if args.dll: shutil.copyfile(producer / 'api.dll', consumer / 'api.dll')
                result = run(f'{tag}-{visibility}-run', [str(consumer / 'consumer.exe')], consumer, expected=None)
                if result.returncode != expected:
                    raise SystemExit(f'{tag} {visibility}: NativeSharp devolvio {result.returncode}, Clang {expected}; '
                                     f'evidencia: {root}')
            print(f'{tag}: control C y plantilla precompilada con y sin encabezado pasaron', flush=True)
for item in CASES:
    for opt in range(4):
        tag, case, producer, expected = prepare_variadic(item, opt)
        clang_shapes = physical_arguments((producer / 'control.ll').read_text(encoding='utf-8'), tag + ' Clang')
        for visibility in ('con-encabezado', 'sin-encabezado'):
            consumer, ir = consume(tag, case, producer, visibility, opt)
            user = ir.split('define internal i1 @__nativesharp_rtti')[0]
            shapes = physical_arguments(user, f'{tag} {visibility}')
            if shapes != clang_shapes:
                raise SystemExit(f'{tag} {visibility}: argumentos {shapes} distintos de Clang {clang_shapes}; '
                                 f'evidencia: {root}')
            run(f'{tag}-{visibility}-build', [args.nsc, '--emit-project-oop-exe', 'Consumer.nsproj',
                '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O' + str(opt), '-o', 'consumer.exe'],
                consumer)
            if args.dll: shutil.copyfile(producer / 'api.dll', consumer / 'api.dll')
            result = run(f'{tag}-{visibility}-run', [str(consumer / 'consumer.exe')], consumer, expected=None)
            if result.returncode != expected:
                raise SystemExit(f'{tag} {visibility}: NativeSharp devolvio {result.returncode}, Clang {expected}; '
                                 f'evidencia: {root}')
        print(f'{tag}: control C, IR de Clang y plantilla precompilada con y sin encabezado coinciden '
              f'({len(clang_shapes)} argumentos fisicos)', flush=True)
for negative in NEGATIVES:
    label, fields = negative[0], negative[1]
    tag = 'negativo-' + label
    case = root / tag
    case.mkdir()
    (case / 'api.h').write_text(header('sysv_abi', fields), encoding='utf-8')
    (case / 'api.c').write_text('#include "api.h"\nT09Ptr t09_get(void) { return 0; }\n', encoding='utf-8')
    clang = run(tag + '-clang', ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c17', '-c', 'api.c', '-o',
                                 'api.obj'], case, expected=1).stdout
    diagnostic = 'requested alignment is less than minimum alignment'
    if diagnostic not in clang:
        raise SystemExit(f'{tag}: Clang no rechazo el record como se esperaba; evidencia: {root}')
    (case / 'api.lib').write_bytes(b'')
    (case / 'api.dll').write_bytes(b'')
    (case / 'Producer.ns').write_text('namespace T09 { public class Bridge { public static int Run<T>(T unused) { '
        'T09R r; ' + negative[3] + ' T09Ptr p = t09_get(); return p(r, 4); } } }\n', encoding='utf-8')
    (case / 'Producer.nsproj').write_text(project('T09Producer', c_reference(), True), encoding='utf-8')
    output = run(tag + '-producer', [args.nsc, '--emit-project-oop-lib', 'Producer.nsproj',
                                     '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-o',
                                     'Producer.lib'], case, expected=1).stdout
    if diagnostic not in output:
        raise SystemExit(f'{tag}: NSC no rechazo el record con el diagnostico de Clang; evidencia: {root}')
    print(f'{tag}: rechazado por Clang y por NSC con el mismo diagnostico', flush=True)
print('OK: records C con atributos de alineacion, __unaligned, #pragma pack, atributos sin efecto en el layout y '
      'alineacion con campos de bits reconstruidos por el replay precompilado (punteros y llamada variadica '
      'sysv_abi); _Alignas menor que la natural rechazado como en Clang; evidencia:', root)
