"""Record regcall de tres float, antes control negativo, comparado ahora con Clang; y rechazo de una firma regcall aun no admitida.

El record {float x, y, z} se expande en tres float y NSC ya lo admite por el plan de Clang:
esta prueba lo compara con un control C, O0-O3, y exige x86_regcallcc y las tres piezas en
el IR. El enum C por valor, antes control negativo, se compara ahora tambien con Clang (T09-003).
El control negativo conserva la comprobacion de rechazo con un enum C++ con ambito por
valor, que Clang compila y NSC todavia no proyecta con regcall.
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
output = run('enum-cpp-nsc', [args.nsc, '--dump-project-oop-llvm', 'Negative.nsproj', '--cpp-clang-root=' + args.clang_root],
             negative, expected=1)
if 'regcall currently supports only int scalar signatures' not in output:
    raise SystemExit('La firma no verificada no fue rechazada como se esperaba; evidencia: ' + str(root))
print('OK: record de tres float y enum C comparados con Clang; firma con enum C++ rechazada; evidencia:', root)
