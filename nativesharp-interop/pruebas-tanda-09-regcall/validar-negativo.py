"""Record regcall de tres float, antes control negativo, comparado ahora con Clang; y rechazo de una firma regcall aun no admitida.

El record {float x, y, z} se expande en tres float y NSC ya lo admite por el plan de Clang:
esta prueba lo compara con un control C, O0-O3, y exige x86_regcallcc y las tres piezas en
el IR. El enum C por valor, antes control negativo, se compara ahora tambien con Clang (T09-003).
El enum C++ con ambito por valor, el siguiente control negativo, tambien se proyecta ya
(T09-003): se compara con el callee de Clang, O0-O3. La union C solo de float por valor
con regcall, el siguiente control negativo, se compara tambien con Clang (T09-003); el rechazo
se conserva con un struct C++ con un campo de bits de ancho cero, que NSC no representa al
expandirlo.
"""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
args = parser.parse_args()
fixture = Path(__file__).resolve().parent
root = Path(tempfile.mkdtemp(prefix='nsc-t09-regcall-negativo-', dir=r'A:\temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)


def run(label, command, cwd, expected=0):
    result = subprocess.run(command, cwd=cwd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, errors='replace')
    (root / (label + '.log')).write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) + '\n'
                                         + result.stdout, encoding='utf-8')
    if (expected == 0 and result.returncode) or (expected != 0 and result.returncode == 0):
        raise SystemExit(label + ': codigo inesperado; evidencia: ' + str(root / (label + '.log')))
    return result.stdout


if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version'], root):
    raise SystemExit('Se requiere Clang 23.1.0')
for opt in range(4):
    case = root / ('tres-float-O' + str(opt)); case.mkdir()
    for name in ('api.h', 'api.c', 'VectorCall.nsproj'):
        shutil.copyfile(fixture / name, case / name)
    for name in ('api.h', 'api.c'):
        path = case / name
        text = path.read_text(encoding='utf-8').replace('int a, int b, int c', 'Unsupported a, int b, int c')
        if name == 'api.h':
            text = text.replace('#pragma once', '#pragma once\ntypedef struct Unsupported { float x,y,z; } Unsupported;')
        else:
            text = '__declspec(selectany) int _fltused = 0;\n' + text.replace('a * 100', '(int)a.x * 100')
        path.write_text(text, encoding='utf-8')
    (case / 'Control.c').write_text('#include "api.h"\n__declspec(selectany) int _fltused = 0;\n'
        'int main(void) { Unsupported value = {3.0f, 0.0f, 0.0f}; return t09_regcall(value, 4, 7) == 347 && '
        'value.x == 3.0f ? 0 : 1; }\n', encoding='utf-8')
    (case / 'Main.ns').write_text('int Main() { Unsupported value; value.x=(float)3; value.y=(float)0; value.z=(float)0; '
        'return t09_regcall(value,4,7)==347 && value.x==(float)3 ? 0 : 1; }\n', encoding='utf-8')
    flags = ['-target', 'x86_64-pc-windows-msvc', '-O' + str(opt)]
    run(f'O{opt}-compile', ['clang.exe', *flags, '-c', 'api.c', '-o', 'api.obj'], case)
    clang_ir = run(f'O{opt}-clang-ir', ['clang.exe', *flags, '-O0', '-S', '-emit-llvm', 'api.c', '-o', '-'], case)
    if not re.search(r'define dso_local x86_regcallcc i32 @__regcall3__t09_regcall\(float [^,]*, float [^,]*, float ', clang_ir):
        raise SystemExit(f'O{opt}: Clang no expande el record en tres float; evidencia: {root}')
    run(f'O{opt}-archive', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], case)
    run(f'O{opt}-control-obj', ['clang.exe', *flags, '-c', 'Control.c', '-o', 'control.obj'], case)
    run(f'O{opt}-control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console', '/nodefaultlib',
                                  '/out:control.exe', 'control.obj', 'api.lib'], case)
    run(f'O{opt}-control-run', [str(case / 'control.exe')], case)
    common = ['VectorCall.nsproj', '--cpp-clang-root=' + args.clang_root]
    ir = run(f'O{opt}-nsc-ir', [args.nsc, '--dump-project-oop-llvm', *common], case)
    if not re.search(r'call x86_regcallcc i32 @__regcall3__t09_regcall\(float [^,]+, float [^,]+, float [^,]+, i32 4, i32 7\)', ir):
        raise SystemExit(f'O{opt}: NSC no usa la llamada regcall de Clang; evidencia: {root}')
    run(f'O{opt}-nsc-build', [args.nsc, '--emit-project-oop-exe', *common, '--no-artifact-cache', '-O' + str(opt),
                               '-o', 'native.exe'], case)
    run(f'O{opt}-nsc-run', [str(case / 'native.exe')], case)
    print(f'O{opt}: record de tres float: control C y NativeSharp correctos', flush=True)
# The former negative input, a C enum by value, is now projected with Clang's plan (T09-003): it is
# compared against a Clang control, O0-O3. The rejection check is kept with a C++ scoped enum by value,
# which Clang compiles and NSC does not project with regcall.
for opt in range(4):
    case = root / ('enum-O' + str(opt)); case.mkdir()
    for name in ('api.h', 'api.c', 'VectorCall.nsproj'):
        shutil.copyfile(fixture / name, case / name)
    for name in ('api.h', 'api.c'):
        path = case / name
        text = path.read_text(encoding='utf-8').replace('int a, int b, int c', 'T09Mode a, int b, int c')
        if name == 'api.h':
            text = text.replace('#pragma once', '#pragma once\ntypedef enum T09Mode { T09ModeA = 1, T09ModeB = 3 } T09Mode;')
        path.write_text(text, encoding='utf-8')
    (case / 'Control.c').write_text('#include "api.h"\nint main(void) { return t09_regcall(T09ModeB, 4, 7) == 347 ? 0 : 1; }\n',
                                    encoding='utf-8')
    (case / 'Main.ns').write_text('int Main() { return t09_regcall(T09ModeB,4,7)==347 ? 0 : 1; }\n', encoding='utf-8')
    flags = ['-target', 'x86_64-pc-windows-msvc', '-O' + str(opt)]
    run(f'enum-O{opt}-compile', ['clang.exe', *flags, '-c', 'api.c', '-o', 'api.obj'], case)
    clang_ir = run(f'enum-O{opt}-clang-ir', ['clang.exe', *flags, '-O0', '-S', '-emit-llvm', 'api.c', '-o', '-'], case)
    if not re.search(r'define dso_local x86_regcallcc i32 @__regcall3__t09_regcall\(i32 ', clang_ir):
        raise SystemExit(f'O{opt}: Clang no pasa el enum como i32 regcall; evidencia: {root}')
    run(f'enum-O{opt}-archive', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], case)
    run(f'enum-O{opt}-control-obj', ['clang.exe', *flags, '-c', 'Control.c', '-o', 'control.obj'], case)
    run(f'enum-O{opt}-control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console', '/nodefaultlib',
                                       '/out:control.exe', 'control.obj', 'api.lib'], case)
    run(f'enum-O{opt}-control-run', [str(case / 'control.exe')], case)
    common = ['VectorCall.nsproj', '--cpp-clang-root=' + args.clang_root]
    ir = run(f'enum-O{opt}-nsc-ir', [args.nsc, '--dump-project-oop-llvm', *common], case)
    if not re.search(r'call x86_regcallcc i32 @__regcall3__t09_regcall\(i32 [^,]+, i32 4, i32 7\)', ir):
        raise SystemExit(f'O{opt}: NSC no usa la llamada regcall de Clang con el enum; evidencia: {root}')
    run(f'enum-O{opt}-nsc-build', [args.nsc, '--emit-project-oop-exe', *common, '--no-artifact-cache', '-O' + str(opt),
                                    '-o', 'native.exe'], case)
    run(f'enum-O{opt}-nsc-run', [str(case / 'native.exe')], case)
    print(f'O{opt}: enum C por valor: control C y NativeSharp correctos', flush=True)
negative = root / 'enum-cpp'; negative.mkdir()
(negative / 'api.h').write_text('#pragma once\nnamespace T09N { enum class Mode : int { A = 1, B = 3 }; '
    'int __attribute__((regcall)) t09_mode(Mode m, int b, int c); }\n', encoding='utf-8')
(negative / 'api.cpp').write_text('#include "api.h"\nnamespace T09N { int __attribute__((regcall)) '
    't09_mode(Mode m, int b, int c) { return (int)m * 100 + b * 10 + c; } }\n', encoding='utf-8')
(negative / 'Main.ns').write_text('int Main() { return T09N.t09_mode(T09N.Mode.B,4,7)==347 ? 0 : 1; }\n', encoding='utf-8')
(negative / 'Negative.nsproj').write_text('<Project><PropertyGroup><Name>Negative</Name></PropertyGroup><ItemGroup>'
    '<Compile Include="Main.ns" /><CppLibraryReference Header="api.h" Language="C++" Standard="c++20" '
    'Library="api.lib" /></ItemGroup></Project>', encoding='utf-8')
run('enum-cpp-compile', ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-c', 'api.cpp', '-o', 'api.obj'],
    negative)
run('enum-cpp-archive', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], negative)

# EN: T09-003. The former negative input, a C++ enum class by value with regcall, is now projected through Clang's
#     plan: the callee compiled by Clang returns the expected value and NSC calls it with x86_regcallcc.
# ES: T09-003. La entrada negativa anterior, un enum class C++ por valor con regcall, ya se proyecta por el plan de
#     Clang: el callee compilado por Clang devuelve el valor esperado y NSC lo llama con x86_regcallcc.
enumcpp_ir = subprocess.run([args.nsc, '--dump-project-oop-llvm', 'Negative.nsproj', '--cpp-clang-root=' + args.clang_root],
                             cwd=negative, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
(root / 'enumcpp-ir.log').write_text('EXIT: ' + str(enumcpp_ir.returncode) + '\n' + enumcpp_ir.stdout, encoding='utf-8')
if enumcpp_ir.returncode or not re.search(r'(?:call|invoke) x86_regcallcc i32 @"\?t09_mode@', enumcpp_ir.stdout):
    raise SystemExit('El enum C++ con regcall no se llama con x86_regcallcc; evidencia: ' + str(root))
for enumcpp_opt in range(4):
    enumcpp_exe = str(negative / ('native-enum-cpp.exe'.replace('.exe', '-O' + str(enumcpp_opt) + '.exe')))
    enumcpp_build = subprocess.run([args.nsc, '--emit-project-oop-exe', 'Negative.nsproj', '--cpp-clang-root=' + args.clang_root,
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
print('OK: record de tres float, enum C y enum C++ comparados con Clang; union solo float comparada con Clang; struct C++ con campo de ancho cero rechazado; evidencia:', root)
