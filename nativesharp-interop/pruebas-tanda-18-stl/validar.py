"""T18-012 y T18-013: especializaciones de la STL de MSVC, una biblioteca de plantillas en encabezados.

La STL del toolset 14.52.36815 se analiza desde sus encabezados (`<array>`, `<limits>`, `<bit>`, `<numeric>`,
`<type_traits>`) y NativeSharp usa especializaciones que Clang instancia: la plantilla de clase con parametro no
tipo `std::array<int, 4>` (miembro `_Elems`, `size()`, `fill` con `in`), miembros estaticos de
`std::numeric_limits<sbyte>` y `<short>` y de `std::integral_constant<int, 5>`, y plantillas de funcion con
argumentos explicitos (`std::gcd<int, int>`, `std::lcm<int, int>`, `std::popcount<uint>`, `std::bit_width<uint>`,
`std::countl_zero<uint>`, `std::has_single_bit<uint>`, `std::rotl<uint>`), y los retornos por referencia de
`std::min`, `std::max`, `std::clamp` (`const T&`) y `std::array::at` (`T&` y `const T&`, T19-007). Cada expresion se
compara con un control C++ de Clang 23.1.0 que la escribe igual, O0-O3. El ejecutable arranca por el runtime de C
(`--crt-startup`). La STL no tiene parte binaria aqui, asi que no hay variante DLL.
"""
import argparse
import os
import subprocess
import tempfile
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
parser.add_argument('--dll', action='store_true')
args = parser.parse_args()
root = Path(tempfile.mkdtemp(prefix='nsc-t18-stl-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
env['NSC_LINKER'] = 'lld-link.exe'
print('Evidencia:', root, flush=True)
if args.dll:
    print('OK: la STL no tiene parte binaria en esta prueba; la variante DLL no aplica', flush=True)
    raise SystemExit(0)
TOOLSET = Path('A:/vc2026-runtime/toolsets/14.52.36815')
SDK = Path('A:/vc2026-runtime/windows-sdk/10.0.28000.0')
CRT = [TOOLSET / 'lib/x64/libcmt.lib', TOOLSET / 'lib/x64/libvcruntime.lib', TOOLSET / 'lib/x64/libcpmt.lib',
       SDK / 'lib/ucrt/x64/libucrt.lib', SDK / 'lib/um/x64/kernel32.lib',
       # X2: the associative containers wait on addresses (WaitOnAddress). / Los contenedores asociativos.
       SDK / 'lib/um/x64/synchronization.lib']
SYSTEM = [TOOLSET / 'include', SDK / 'include/ucrt']
HEADER = ('#pragma once\n#include <algorithm>\n#include <array>\n#include <bit>\n#include <limits>\n#include <numeric>\n'
          '#include <type_traits>\n#include <vector>\n#include <string>\n#include <list>\n#include <map>\n'
          '#include <unordered_map>\n')
# (name, NativeSharp body, C++ body)
CASES = [
    ('array', 'std.array<int, 4> a = new std.array<int, 4>(); a._Elems[2] = 7; return (int)a.size() * 10 + a._Elems[2];',
     'std::array<int, 4> a{}; a._Elems[2] = 7; return (int)a.size() * 10 + a._Elems[2];'),
    ('array-fill', 'std.array<int, 4> a = new std.array<int, 4>(); int v = 3; a.fill(in v); return a._Elems[1] + (int)a.size();',
     'std::array<int, 4> a{}; int v = 3; a.fill(v); return a._Elems[1] + (int)a.size();'),
    ('array-local', 'std.array<int, 4> a; a._Elems[0] = 5; return a._Elems[0];',
     'std::array<int, 4> a; a._Elems[0] = 5; return a._Elems[0];'),
    ('limits', 'return std.numeric_limits<sbyte>.max() + std.numeric_limits<short>.digits;',
     'return std::numeric_limits<signed char>::max() + std::numeric_limits<short>::digits;'),
    ('integral-constant', 'return std.integral_constant<int, 5>.value;', 'return std::integral_constant<int, 5>::value;'),
    ('numeric', 'return std.gcd<int, int>(84, 36) * 10 + std.lcm<int, int>(4, 6);',
     'return std::gcd<int, int>(84, 36) * 10 + std::lcm<int, int>(4, 6);'),
    ('bits', 'return std.popcount<uint>(255) + std.bit_width<uint>(255) * 10 + std.countl_zero<uint>(1) + '
             '(std.has_single_bit<uint>(8) ? 100 : 0) + (int)std.rotl<uint>(1, 3);',
     'return std::popcount<unsigned>(255) + std::bit_width<unsigned>(255) * 10 + std::countl_zero<unsigned>(1) + '
     '(std::has_single_bit<unsigned>(8) ? 100 : 0) + (int)std::rotl<unsigned>(1, 3);'),
    # T19-007: results by reference. / Resultados por referencia.
    ('min-max-clamp', 'int a = 4; int b = 9; int c = 12; return std.min<int>(in a, in b) * 100 + '
                      'std.max<int>(in a, in b) * 10 + std.clamp<int>(in c, in a, in b);',
     'int a = 4; int b = 9; int c = 12; return std::min<int>(a, b) * 100 + std::max<int>(a, b) * 10 + '
     'std::clamp<int>(c, a, b);'),
    ('array-at', 'std.array<int, 4> a = new std.array<int, 4>(); a.at(2) = 7; a.at(1) = a.at(2) + 1; '
                 'return a._Elems[1] * 10 + a.at(2);',
     'std::array<int, 4> a{}; a.at(2) = 7; a.at(1) = a.at(2) + 1; return a._Elems[1] * 10 + a.at(2);'),
    # X2: foreach over a C++ range (begin()/end(), the iterator's ==, != rewritten from it, * and ++), as C++'s
    #     range-for. / foreach sobre un rango C++, como el range-for de C++.
    ('array-foreach', 'std.array<int, 4> a = new std.array<int, 4>(); a._Elems[0] = 3; a._Elems[1] = 4; a._Elems[2] = 5; '
                      'a._Elems[3] = 6; int s = 0; foreach (var x in a) { s += x; } return s;',
     'std::array<int, 4> a{}; a._Elems[0] = 3; a._Elems[1] = 4; a._Elems[2] = 5; a._Elems[3] = 6; int s = 0; '
     'for (int x : a) { s += x; } return s;'),
    ('vector-foreach', 'std.vector<int> v = new std.vector<int>(); int a = 7; int b = 9; v.push_back(in a); '
                       'v.push_back(in b); int s = 0; foreach (var x in v) { s += x; } return s + (int)v.size();',
     'std::vector<int> v; int a = 7; int b = 9; v.push_back(a); v.push_back(b); int s = 0; for (int x : v) { s += x; } '
     'return s + (int)v.size();'),
    # X2: std::string named as `std.string`, built from a literal (constructor `const char* const`), `append`
    #     chained as a statement (its `basic_string&` result discarded), `+=` with a literal and `[]`.
    #     / std::string desde un literal, `append` encadenado, `+=` con literal e indice.
    ('string', 'std.string s = new std.string("hola"); s.append("mundo").append("!"); s += "xy"; '
               'return (int)s.size() * 10 + (int)s[0] - 100;',
     'std::string s("hola"); s.append("mundo").append("!"); s += "xy"; return (int)s.size() * 10 + (int)s[0] - 100;'),
    # X2: std's free operator function templates (`operator==(const basic_string&, const char*)`) deduced from the
    #     operands, and C++20's `!=` rewritten from them. / Plantillas de operador libres de std y `!=` reescrito.
    ('string-compare', 'std.string s = new std.string("hola"); std.string t = new std.string("hol"); '
                       'return (s == "hola" ? 1 : 0) + (s != t ? 2 : 0) + (s == t ? 4 : 0) + (s != "hola" ? 8 : 0);',
     'std::string s("hola"); std::string t("hol"); '
     'return (s == "hola" ? 1 : 0) + (s != t ? 2 : 0) + (s == t ? 4 : 0) + (s != "hola" ? 8 : 0);'),
    # X2: C++20's `<`, `<=`, `>`, `>=` rewritten from std's `operator<=>`. / Relacionales reescritos desde `<=>`.
    ('string-order', 'std.string s = new std.string("abc"); std.string t = new std.string("abd"); '
                     'return (s < t ? 1 : 0) + (s <= t ? 2 : 0) + (s > t ? 4 : 0) + (s >= t ? 8 : 0) + (t > s ? 16 : 0);',
     'std::string s("abc"); std::string t("abd"); '
     'return (s < t ? 1 : 0) + (s <= t ? 2 : 0) + (s > t ? 4 : 0) + (s >= t ? 8 : 0) + (t > s ? 16 : 0);'),
    # X2: std::list, std::map and std::unordered_map (`operator[](const K&)` / `(K&&)`, members C++ never instantiates
    #     left out of the emission). / list, map y unordered_map.
    ('list', 'std.list<int> l = new std.list<int>(); int a = 40; int b = 2; l.push_back(in a); l.push_back(in b); '
             'return l.front() + l.back() + (int)l.size();',
     'std::list<int> l; int a = 40; int b = 2; l.push_back(a); l.push_back(b); return l.front() + l.back() + (int)l.size();'),
    ('map', 'std.map<int, int> m = new std.map<int, int>(); int k = 1; m[k] = 40; m[2] = 2; return m[k] + m[2] + (int)m.size();',
     'std::map<int, int> m; int k = 1; m[k] = 40; m[2] = 2; return m[k] + m[2] + (int)m.size();'),
    ('unordered-map', 'std.unordered_map<int, int> m = new std.unordered_map<int, int>(); int k = 7; m[k] = 30; m[9] = 5; '
                      'return m[k] + m[9] + (int)m.size();',
     'std::unordered_map<int, int> m; int k = 7; m[k] = 30; m[9] = 5; return m[k] + m[9] + (int)m.size();'),
]


def run(label, command, cwd, expected=0, extra_env=None):
    result = subprocess.run(command, cwd=cwd, env={**env, **(extra_env or {})}, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    log = root / (label + '.log')
    log.write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' + result.stdout,
                   encoding='utf-8')
    if expected is not None and ((expected == 0 and result.returncode) or (expected != 0 and result.returncode == 0)):
        print(result.stdout[-3000:], flush=True)
        raise SystemExit(label + ': codigo ' + str(result.returncode) + ' inesperado; evidencia: ' + str(log))
    return result


if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version'], root).stdout:
    raise SystemExit('Se requiere Clang 23.1.0')
system = ''.join('<IncludeDirectory Path="' + str(p).replace('\\', '/') + '" Kind="System" />' for p in SYSTEM)
crt = ['--crt-startup'] + [item for path in CRT for item in ('--link-lib', str(path))]
for opt in range(4):
    case = root / f'O{opt}'
    case.mkdir()
    (case / 'api.h').write_text(HEADER, encoding='utf-8')
    functions = ''.join(f'static int case_{index}() {{ {cpp} }}\n' for index, (_, _, cpp) in enumerate(CASES))
    calls = ' + '.join(f'case_{index}() * {10 ** (index % 3)}' for index in range(len(CASES)))
    (case / 'control.cpp').write_text('#include "api.h"\n' + functions + 'int main() { return ' + calls + '; }\n',
                                      encoding='utf-8')
    run(f'O{opt}-control', ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-O' + str(opt),
                            *[x for p in SYSTEM for x in ('-isystem', str(p))], '-c', 'control.cpp', '-o', 'control.obj'], case)
    run(f'O{opt}-control-link', ['lld-link.exe', '/nologo', '/subsystem:console', '/out:control.exe', 'control.obj',
                                 *[str(p) for p in CRT]], case)
    control = run(f'O{opt}-control-run', [str(case / 'control.exe')], case, expected=None).returncode
    methods = ''.join(f'int Case{index}() {{ {ns} }}\n' for index, (_, ns, _) in enumerate(CASES))
    ns_calls = ' + '.join(f'Case{index}() * {10 ** (index % 3)}' for index in range(len(CASES)))
    (case / 'Main.ns').write_text(methods + 'int Main() { return ' + ns_calls + '; }\n', encoding='utf-8')
    (case / 'App.nsproj').write_text(
        '<Project><PropertyGroup><Name>T18Stl</Name></PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
        '<CppLibraryReference Header="api.h" Language="C++" Standard="c++20">' + system + '</CppLibraryReference>'
        '</ItemGroup></Project>', encoding='utf-8')
    run(f'O{opt}-nsc', [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
                        '--no-artifact-cache', '-O' + str(opt), *crt, '-o', 'app.exe'], case)
    result = run(f'O{opt}-run', [str(case / 'app.exe')], case, expected=None).returncode
    if result != control:
        raise SystemExit(f'O{opt}: NativeSharp devolvio {result}, Clang {control}; evidencia: {root}')
    print(f'O{opt}: NativeSharp y Clang devuelven {result} ({len(CASES)} grupos de especializaciones)', flush=True)
print('OK: especializaciones de la STL consumidas desde NativeSharp, frente a Clang; evidencia:', root)
