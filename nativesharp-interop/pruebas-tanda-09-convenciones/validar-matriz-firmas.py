"""T09-003: matriz de convenciones por clase de firma, Clang 23.1 frente a NSC en Windows x64.

Para cada convencion que NSC conoce y cada clase de firma (escalares de cada ancho,
puntero, records compactos, indirectos, flotantes y mixtos) compila con Clang una
funcion C, la llama desde NativeSharp directamente y por un puntero a funcion devuelto
por C, en biblioteca estatica y O0. Si NSC acepta la llamada, exige la convencion LLVM
de Clang en la declaracion o en la llamada indirecta y comprueba los valores devueltos
y que el argumento original no cambie. Una aceptacion incorrecta hace fallar la prueba;
los rechazos se registran en la matriz.
"""
import argparse
import os
import re
import subprocess
import tempfile
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
args = parser.parse_args()
root = Path(tempfile.mkdtemp(prefix='nsc-t09-matriz-firmas-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)

CONVENTIONS = ('cdecl', 'vectorcall', 'regcall', 'sysv_abi', 'preserve_most', 'preserve_all', 'preserve_none')
CC_TOKEN = re.compile(r'\b(x86_\w+cc|win64cc|preserve_\w+cc|swift\w*cc|intel_ocl_bicc|fastcc|coldcc|cc \d+)\b')
INDIRECT = re.compile(r'call (?:(\S+cc|cc \d+) )?[^@%\n]*%[\w.]+\(')
RECORDS = ('typedef struct R8 { int a; int b; } R8;\n'
           'typedef struct R12 { int a; int b; int c; } R12;\n'
           'typedef struct R36 { int values[9]; } R36;\n'
           'typedef struct RF { float x; float y; } RF;\n'
           'typedef struct RM { double d; int i; } RM;\n')
SCALARS = (('i8', 'signed char', 'sbyte', '(sbyte)(-7)'), ('u8', 'unsigned char', 'byte', '(byte)250'),
           ('i16', 'short', 'short', '(short)(-1000)'), ('u16', 'unsigned short', 'ushort', '(ushort)65000'),
           ('i32', 'int', 'int', '-100000'), ('u32', 'unsigned int', 'uint', '(uint)4000000000'),
           ('i64', 'long long', 'long', '-5000000000L'), ('u64', 'unsigned long long', 'ulong', '(ulong)10000000000'),
           ('bool', '_Bool', 'bool', 'true'), ('f32', 'float', 'float', '2.5f'), ('f64', 'double', 'double', '2.25'))
CLASSES = []
for key, c_type, ns_type, literal in SCALARS:
    CLASSES.append((key, c_type, f'{c_type} v', 'return v;', f'{ns_type} v = {literal}; if (CALL(v) != v) return 1;'))
CLASSES += [
    ('puntero', 'int*', 'int* p, int d', '*p += d; return p;',
     'int v = 5; int* r = CALL(&v, 3); if (r != &v || v != 8) return 1;'),
    ('record8', 'R8', 'R8 v', 'v.a += 1; v.b += 2; return v;',
     'R8 v; v.a = 1; v.b = 2; R8 r = CALL(v); if (r.a != 2 || r.b != 4 || v.a != 1 || v.b != 2) return 1;'),
    ('record12', 'R12', 'R12 v', 'v.a += 1; v.b += 2; v.c += 3; return v;',
     'R12 v; v.a = 1; v.b = 2; v.c = 3; R12 r = CALL(v); '
     'if (r.a != 2 || r.b != 4 || r.c != 6 || v.c != 3) return 1;'),
    ('record36', 'R36', 'R36 v', 'v.values[0] += 1; v.values[8] += 2; return v;',
     'R36 v; v.values[0] = 1; v.values[8] = 9; R36 r = CALL(v); '
     'if (r.values[0] != 2 || r.values[8] != 11 || v.values[8] != 9) return 1;'),
    ('record-flotante', 'RF', 'RF v', 'v.x += 0.5f; v.y += 1.5f; return v;',
     'RF v; v.x = 1.0f; v.y = 2.0f; RF r = CALL(v); if (r.x != 1.5f || r.y != 3.5f || v.y != 2.0f) return 1;'),
    ('record-mixto', 'RM', 'RM v', 'v.d += 0.25; v.i += 7; return v;',
     'RM v; v.d = 1.0; v.i = 2; RM r = CALL(v); if (r.d != 1.25 || r.i != 9 || v.i != 2) return 1;'),
    ('mixta', 'double', 'float a, int* p, R8 r, double d', 'return a + *p + r.a + r.b + d;',
     'int value = 4; R8 r; r.a = 10; r.b = 20; if (CALL(0.5f, &value, r, 0.25) != 34.75) return 1;'),
]


def execute(label, command, cwd):
    result = subprocess.run(command, cwd=cwd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, errors='replace')
    (cwd / (label + '.log')).write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) + '\n'
                                        + result.stdout, encoding='utf-8')
    return result.returncode, result.stdout


def convention_of(line):
    found = CC_TOKEN.search(line.split('@', 1)[0]) if line else None
    return found.group(1) if found else 'ccc'


def clang_side(case, convention, item):
    key, result, parameters, body, check = item
    attribute = f'__attribute__(({convention}))'
    header = (RECORDS + f'{result} {attribute} t09_f({parameters});\n'
              f'typedef {result} ({attribute} *T09Ptr)({parameters});\nT09Ptr t09_get(void);\n')
    (case / 'api.h').write_text('#pragma once\n' + header, encoding='utf-8')
    (case / 'api.c').write_text('#include "api.h"\n__declspec(selectany) int _fltused = 0;\n'
        f'{result} {attribute} t09_f({parameters}) {{ {body} }}\nT09Ptr t09_get(void) {{ return t09_f; }}\n',
        encoding='utf-8')
    flags = ['-target', 'x86_64-pc-windows-msvc', '-std=c17', '-O0']
    code, text = execute('clang-ir', ['clang.exe', *flags, '-S', '-emit-llvm', 'api.c', '-o', 'api.ll'], case)
    if code:
        return None
    ir = (case / 'api.ll').read_text(encoding='utf-8', errors='replace')
    line = next((l for l in ir.splitlines() if l.startswith('define ') and 't09_f' in l), '')
    symbol = line.split('@', 1)[1].split('(', 1)[0] if line else ''
    execute('clang-obj', ['clang.exe', *flags, '-c', 'api.c', '-o', 'api.obj'], case)
    execute('lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], case)
    return convention_of(line), symbol


def nsc_side(case, expected, item, indirect):
    label = 'puntero' if indirect else 'directa'
    check = item[4].replace('CALL', 'p' if indirect else 't09_f')
    prefix = 'T09Ptr p = t09_get(); ' if indirect else ''
    (case / (label + '.ns')).write_text('int Main() { ' + prefix + check + ' return 0; }\n', encoding='utf-8')
    (case / (label + '.nsproj')).write_text('<Project><PropertyGroup><Name>Fixture</Name></PropertyGroup><ItemGroup>'
        '<Compile Include="' + label + '.ns" /><CppLibraryReference Header="api.h" Language="C" Standard="c17" '
        'Library="api.lib" /></ItemGroup></Project>', encoding='utf-8')
    common = [label + '.nsproj', '--cpp-clang-root=' + args.clang_root]
    code, text = execute(label + '-ir', [args.nsc, '--dump-project-oop-llvm', *common], case)
    if code:
        return 'rechazo', next((l.strip() for l in text.splitlines() if 'error' in l), 'error')[:200]
    if indirect:
        user_code = text.split('define internal i1 @__nativesharp_rtti')[0]
        found = {match.group(1) or 'ccc' for match in INDIRECT.finditer(user_code) if 't09_get' not in match.group(0)}
        if expected[0] not in found or any(cc != 'ccc' and cc != expected[0] for cc in found):
            return 'FALLO', f'convenciones indirectas {sorted(found)!r}'
    else:
        line = next((l for l in text.splitlines() if l.startswith('declare') and '@' + expected[1] + '(' in l), '')
        if not line or convention_of(line) != expected[0]:
            return 'FALLO', f'declaracion {line.strip()!r}'
    code, text = execute(label + '-build', [args.nsc, '--emit-project-oop-exe', *common, '--no-artifact-cache',
                                            '-O0', '-o', label + '.exe'], case)
    if code:
        return 'FALLO', 'IR aceptado pero construccion fallida'
    code, _ = execute(label + '-run', [str(case / (label + '.exe'))], case)
    return ('pasa' if code == 0 else 'FALLO'), ('' if code == 0 else f'salida {code}')


rows, failures = [], 0
for convention in CONVENTIONS:
    for item in CLASSES:
        case = root / (convention + '-' + item[0])
        case.mkdir()
        expected = clang_side(case, convention, item)
        if expected is None:
            rows.append((convention, item[0], 'Clang rechaza', '', '', ''))
            continue
        direct = nsc_side(case, expected, item, False)
        pointer = nsc_side(case, expected, item, True)
        failures += (direct[0] == 'FALLO') + (pointer[0] == 'FALLO')
        rows.append((convention, item[0], direct[0], direct[1], pointer[0], pointer[1]))
        print(convention, item[0], '|', direct[0], direct[1], '|', pointer[0], pointer[1], flush=True)
report = ['| Convención | Firma | Directa | Detalle | Por puntero | Detalle |', '|---|---|---|---|---|---|']
report += ['| ' + ' | '.join(str(cell).replace('|', '/') for cell in row) + ' |' for row in rows]
(root / 'matriz.md').write_text('\n'.join(report) + '\n', encoding='utf-8')
accepted = sum((row[2] == 'pasa') + (row[4] == 'pasa') for row in rows)
rejected = sum((row[2] == 'rechazo') + (row[4] == 'rechazo') for row in rows)
print(f'celdas: {2 * len(rows)}; pasan: {accepted}; rechazos: {rejected}; fallos: {failures}', flush=True)
if failures:
    raise SystemExit(f'{failures} aceptaciones incorrectas; evidencia: {root}')
print('OK: matriz de firmas; evidencia:', root)
