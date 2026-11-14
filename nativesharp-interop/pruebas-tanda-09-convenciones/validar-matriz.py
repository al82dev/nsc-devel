"""Inventario de convenciones de llamada: Clang 23.1 frente a NSC en Windows x64.

Para cada atributo de convencion y para C y C++ compila con Clang una funcion
int(int, int) y registra si Clang la acepta, si ignora el atributo y la
convencion LLVM que emite. Despues consume la funcion desde NativeSharp con
llamada directa, O0 y biblioteca estatica, y registra si NSC la rechaza o la
acepta; si la acepta, exige la misma convencion LLVM y simbolo que Clang y el
resultado correcto. Repite la comprobacion con una llamada por un puntero a
funcion devuelto por C/C++ y exige la convencion en la llamada indirecta. Una aceptacion con convencion distinta es un fallo.
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
root = Path(tempfile.mkdtemp(prefix='nsc-t09-convenciones-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)

CONVENTIONS = ('cdecl', 'stdcall', 'fastcall', 'thiscall', 'pascal', 'vectorcall', 'regcall', 'ms_abi',
               'sysv_abi', 'preserve_most', 'preserve_all', 'preserve_none', 'swiftcall',
               'swiftasynccall', 'intel_ocl_bicc')
CC_TOKEN = re.compile(r'\b(x86_\w+cc|win64cc|preserve_\w+cc|swift\w*cc|intel_ocl_bicc|fastcc|coldcc|cc \d+)\b')


def execute(label, command, cwd):
    result = subprocess.run(command, cwd=cwd, env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    (cwd / (label + '.log')).write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode)
                                        + '\n' + result.stdout, encoding='utf-8')
    return result.returncode, result.stdout


def symbol_line(ir, verb, name):
    for line in ir.splitlines():
        if line.startswith(verb + ' ') and name in line and '(' in line:
            return line
    return ''


def convention_of(line):
    found = CC_TOKEN.search(line.split('@', 1)[0]) if line else None
    return found.group(1) if found else 'ccc'


def clang_side(case, language, convention):
    source = 'api.cpp' if language == 'C++' else 'api.c'
    open_ns, close_ns = ('namespace T09Cc {\n', '}\n') if language == 'C++' else ('', '')
    declaration = f'int __attribute__(({convention})) t09_cc(int a, int b)'
    pointer = f'typedef int (__attribute__(({convention})) *T09CcPtr)(int, int);\nT09CcPtr t09_cc_get(void)'
    (case / 'api.h').write_text('#pragma once\n' + open_ns + declaration + ';\n' + pointer + ';\n' + close_ns,
                                encoding='utf-8')
    (case / source).write_text('#include "api.h"\n' + open_ns + declaration + ' { return a * 10 + b; }\n'
                               + pointer + ' { return t09_cc; }\n' + close_ns, encoding='utf-8')
    flags = ['-target', 'x86_64-pc-windows-msvc', '-O0'] + (['-std=c++20'] if language == 'C++' else ['-std=c17'])
    code, text = execute('clang-ir', ['clang.exe', *flags, '-S', '-emit-llvm', source, '-o', 'api.ll'], case)
    if code:
        return None, 'Clang rechaza: ' + next((l for l in text.splitlines() if 'error' in l), 'error')
    ignored = 'ignored' in text or 'not supported' in text
    ir = (case / 'api.ll').read_text(encoding='utf-8', errors='replace')
    line = symbol_line(ir, 'define', 't09_cc')
    symbol = line.split('@', 1)[1].split('(', 1)[0] if line else ''
    execute('clang-obj', ['clang.exe', *flags, '-c', source, '-o', 'api.obj'], case)
    execute('lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], case)
    return (convention_of(line), symbol, ignored), ''


INDIRECT = re.compile(r'call (?:(\S+cc|cc \d+) )?(?:noundef )?i32 %[\w.]+\(i32')


def nsc_side(case, language, expected, indirect):
    scope = 'T09Cc.' if language == 'C++' else ''
    body = (scope + 'T09CcPtr p = ' + scope + 't09_cc_get(); return p(2, 3) == 23 ? 0 : 1;') if indirect else \
        ('return ' + scope + 't09_cc(2, 3) == 23 ? 0 : 1;')
    label = 'puntero' if indirect else 'directa'
    (case / (label + '.ns')).write_text('int Main() { ' + body + ' }\n', encoding='utf-8')
    standard = 'c++20' if language == 'C++' else 'c17'
    (case / (label + '.nsproj')).write_text('<Project><PropertyGroup><Name>Fixture</Name></PropertyGroup><ItemGroup>'
        '<Compile Include="' + label + '.ns" /><CppLibraryReference Header="api.h" Language="' + language
        + '" Standard="' + standard + '" Library="api.lib" /></ItemGroup></Project>', encoding='utf-8')
    common = [label + '.nsproj', '--cpp-clang-root=' + args.clang_root]
    code, text = execute(label + '-nsc-ir', [args.nsc, '--dump-project-oop-llvm', *common], case)
    if code:
        first = next((l.strip() for l in text.splitlines() if 'error' in l), 'error')
        return 'rechazo', first
    if indirect:
        found = {match.group(1) or 'ccc' for match in INDIRECT.finditer(text)}
        if found != {expected[0]}:
            return 'FALLO', f'NSC acepta la llamada por puntero con convenciones {sorted(found)!r}'
    else:
        line = symbol_line(text, 'declare', expected[1]) if expected[1] else ''
        convention = convention_of(line)
        if not line or convention != expected[0]:
            return 'FALLO', f'NSC acepta con convencion {convention!r} y declaracion {line.strip()!r}'
    code, text = execute(label + '-nsc-build', [args.nsc, '--emit-project-oop-exe', *common, '--no-artifact-cache',
                                                '-O0', '-o', label + '.exe'], case)
    if code:
        return 'FALLO', 'IR aceptado pero construccion fallida'
    code, _ = execute(label + '-run', [str(case / (label + '.exe'))], case)
    return ('acepta' if code == 0 else 'FALLO'), ('resultado correcto' if code == 0 else f'salida {code}')


rows, failures = [], 0
for language in ('C', 'C++'):
    for convention in CONVENTIONS:
        case = root / (language.replace('+', 'p') + '-' + convention)
        case.mkdir()
        observed, why = clang_side(case, language, convention)
        if observed is None:
            rows.append((language, convention, why, '-', 'no evaluado', '-', 'no evaluado'))
            continue
        clang_text = f'{observed[0]} @{observed[1]}' + (' (atributo ignorado)' if observed[2] else '')
        state, detail = nsc_side(case, language, observed, False)
        pointer_state, pointer_detail = nsc_side(case, language, observed, True)
        failures += (state == 'FALLO') + (pointer_state == 'FALLO')
        rows.append((language, convention, clang_text, state, detail, pointer_state, pointer_detail))
        print(language, convention, '|', clang_text, '|', state, '|', detail, '|', pointer_state, '|',
              pointer_detail, flush=True)
report = ['| Lenguaje | Atributo | Clang 23.1 O0 | Directa | Detalle | Por puntero | Detalle |',
          '|---|---|---|---|---|---|---|']
report += ['| ' + ' | '.join(str(cell).replace('|', '/') for cell in row) + ' |' for row in rows]
(root / 'matriz.md').write_text('\n'.join(report) + '\n', encoding='utf-8')
print('\n'.join(report), flush=True)
if failures:
    raise SystemExit(f'{failures} aceptaciones con convencion o resultado distintos; evidencia: {root}')
print('OK: matriz de convenciones; evidencia:', root)
