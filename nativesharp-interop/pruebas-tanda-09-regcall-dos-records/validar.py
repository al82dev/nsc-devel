"""Contrasta dos records expandidos regcall, con control C independiente."""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument('--nsc', required=True)
p.add_argument('--clang-root', required=True)
p.add_argument('--indirecta', action='store_true')
args = p.parse_args()
src = Path(__file__).resolve().parent
root = Path(tempfile.mkdtemp(prefix='nsc-t09-two-records-', dir=r'A:\temp'))
env = dict(os.environ, TEMP=str(root), TMP=str(root))
print('Evidencia:', root, flush=True)
def run(name, command):
    result = subprocess.run(command, cwd=root, env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    (root/(name+'.log')).write_text('COMMAND: '+repr(command)+'\nEXIT: '+str(result.returncode)+'\n'+result.stdout, encoding='utf-8')
    if result.returncode: raise SystemExit(name+': fallo; evidencia: '+str(root/(name+'.log')))
    return result.stdout
if 'clang version 23.1.0' not in run('version', ['clang.exe', '--version']):
    raise SystemExit('Se requiere Clang 23.1.0')
for mode in ('estatica', 'dll'):
  for opt in range(4):
    case = root/(mode+'-O'+str(opt))
    case.mkdir()
    for name in ('api.h', 'api-dll.h', 'api.c', 'Control.c', 'Main.ns', 'Probe.nsproj', 'ProbeDll.nsproj'):
        shutil.copyfile(src/name, case/name)
    if args.indirecta:
        shutil.copyfile(src/'MainIndirect.ns', case/'Main.ns')
    flags = ['-target', 'x86_64-pc-windows-msvc', '-std=c11', '-O'+str(opt)]
    prefix = mode+'-O'+str(opt)+'-'
    producer_flags = ['-DAPI_DLL_BUILD'] if mode == 'dll' else []
    run(prefix+'producer', ['clang.exe', *flags, *producer_flags, '-c', str(case/'api.c'), '-o', str(case/'api.obj')])
    if mode == 'dll':
        library, project, control_flags = 'api-dll.lib', 'ProbeDll.nsproj', ['-DAPI_DLL_IMPORT']
        run(prefix+'dll', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib',
            '/export:__regcall3__combine', '/export:get_combine', '/out:'+str(case/'api.dll'),
            '/implib:'+str(case/library), str(case/'api.obj')])
    else:
        library, project, control_flags = 'api.lib', 'Probe.nsproj', []
        run(prefix+'archive', ['llvm-lib.exe', '/nologo', '/out:'+str(case/library), str(case/'api.obj')])
    run(prefix+'control-object', ['clang.exe', *flags, *control_flags, '-c', str(case/'Control.c'), '-o', str(case/'control.obj')])
    run(prefix+'control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console', '/nodefaultlib',
                         '/out:'+str(case/'control.exe'), str(case/'control.obj'), str(case/library)])
    run(prefix+'control-run', [str(case/'control.exe')])
    llvm = run(prefix+'llvm', [args.nsc, '--dump-project-oop-llvm', str(case/project),
               '--cpp-clang-root='+args.clang_root])
    pieces = r',\s*'.join([r'float\s+[^,)]+']*6) + r',\s*i32\s+[^,)]+'
    if not re.search(r'call\s+x86_regcallcc\s+\{[^\n]+?\}\s+@(?:"\\01)?__regcall3__combine\('+pieces+r'\)', llvm):
        raise SystemExit(prefix+'no se encontró la llamada física; evidencia: '+str(root))
    if args.indirecta and not re.search(r'call\s+x86_regcallcc\s+\{[^\n]+?\}\s+%[^\s(]+\('+pieces+r'\)', llvm):
        raise SystemExit(prefix+'no se encontró la llamada por puntero; evidencia: '+str(root))
    run(prefix+'ns-build', [args.nsc, '--emit-project-oop-exe', str(case/project),
                     '--cpp-clang-root='+args.clang_root, '--no-artifact-cache', '-O'+str(opt), '-o', str(case/'out.exe')])
    run(prefix+'ns-run', [str(case/'out.exe')])
    print(prefix+'control C y NativeSharp correctos', flush=True)
print('OK: ocho comparaciones estática y DLL', flush=True)
# The former negative input (Triple, Other, int) is admitted through the Clang plan by the existing
# expanded-record path: it is now compared against Clang with its own implementation.
case = root/'forma-abi-distinta'
case.mkdir()
for name in ('api.h', 'Main.ns', 'Probe.nsproj'):
    shutil.copyfile(src/name, case/name)
header = (case/'api.h').read_text(encoding='utf-8')
header = header.replace('API Triple __attribute__((regcall)) combine(Triple a, Triple b, int bias);',
    'typedef struct Other { Pair pair; int z; } Other;\n'
    'Triple __attribute__((regcall)) combine(Triple a, Other b, int bias);')
header = header.replace('typedef Triple (__attribute__((regcall)) *CombineFn)(Triple, Triple, int);\nAPI CombineFn get_combine(void);\n', '')
(case/'api.h').write_text(header, encoding='utf-8')
(case/'api.c').write_text('#include "api.h"\n__declspec(selectany) int _fltused = 0;\n'
    'Triple __attribute__((regcall)) combine(Triple a, Other b, int bias) {\n'
    '    a.pair.x += b.pair.x + (float)bias; a.pair.y += b.pair.y; a.z += (float)b.z; return a;\n}\n', encoding='utf-8')
(case/'Control.c').write_text('#include "api.h"\n__declspec(selectany) int _fltused = 0;\nint main(void) {\n'
    '    Triple a = {{10.0f, 20.0f}, 30.0f}; Other b = {{1.0f, 2.0f}, 3};\n'
    '    Triple c = combine(a, b, 7);\n'
    '    return !(c.pair.x == 18.0f && c.pair.y == 22.0f && c.z == 33.0f && a.pair.x == 10.0f && b.z == 3);\n}\n',
    encoding='utf-8')
main = (case/'Main.ns').read_text(encoding='utf-8').replace('Triple b;', 'Other b;').replace('b.z=(float)3;', 'b.z=3;')
(case/'Main.ns').write_text(main, encoding='utf-8')
for opt in range(4):
    flags = ['-target', 'x86_64-pc-windows-msvc', '-std=c11', '-O'+str(opt)]
    prefix = 'forma-abi-distinta-O'+str(opt)+'-'
    run(prefix+'producer', ['clang.exe', *flags, '-c', str(case/'api.c'), '-o', str(case/'api.obj')])
    run(prefix+'archive', ['llvm-lib.exe', '/nologo', '/out:'+str(case/'api.lib'), str(case/'api.obj')])
    run(prefix+'control-object', ['clang.exe', *flags, '-c', str(case/'Control.c'), '-o', str(case/'control.obj')])
    run(prefix+'control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console', '/nodefaultlib',
                                '/out:'+str(case/'control.exe'), str(case/'control.obj'), str(case/'api.lib')])
    run(prefix+'control-run', [str(case/'control.exe')])
    llvm = run(prefix+'llvm', [args.nsc, '--dump-project-oop-llvm', str(case/'Probe.nsproj'), '--cpp-clang-root='+args.clang_root])
    if not re.search(r'call\s+x86_regcallcc\s+\{[^\n]+?\}\s+@__regcall3__combine\(', llvm):
        raise SystemExit(prefix+'no se encontró la llamada regcall; evidencia: '+str(root))
    run(prefix+'ns-build', [args.nsc, '--emit-project-oop-exe', str(case/'Probe.nsproj'), '--cpp-clang-root='+args.clang_root,
                            '--no-artifact-cache', '-O'+str(opt), '-o', str(case/'out.exe')])
    run(prefix+'ns-run', [str(case/'out.exe')])
    print(prefix+'control C y NativeSharp correctos', flush=True)
# The C enum input, the former rejection, is projected with Clang's plan since T09-003: it is compared
# against a Clang control, O0-O3. The rejection check is kept with a C++ scoped enum by value.
case = root/'enum-c'
case.mkdir()
(case/'api.h').write_text('#pragma once\ntypedef enum Mode { ModeA = 1, ModeB = 2 } Mode;\n'
    'int __attribute__((regcall)) combine_mode(Mode mode, int bias);\n', encoding='utf-8')
(case/'api.c').write_text('#include "api.h"\nint __attribute__((regcall)) combine_mode(Mode mode, int bias) { return (int)mode * 10 + bias; }\n',
    encoding='utf-8')
(case/'Control.c').write_text('#include "api.h"\nint main(void) { return combine_mode(ModeB, 3) == 23 ? 0 : 1; }\n', encoding='utf-8')
(case/'Main.ns').write_text('int Main() { return combine_mode(ModeB, 3) == 23 ? 0 : 1; }\n', encoding='utf-8')
shutil.copyfile(src/'Probe.nsproj', case/'Probe.nsproj')
for opt in range(4):
    flags = ['-target', 'x86_64-pc-windows-msvc', '-std=c11', '-O'+str(opt)]
    prefix = 'enum-c-O'+str(opt)+'-'
    run(prefix+'producer', ['clang.exe', *flags, '-c', str(case/'api.c'), '-o', str(case/'api.obj')])
    run(prefix+'archive', ['llvm-lib.exe', '/nologo', '/out:'+str(case/'api.lib'), str(case/'api.obj')])
    run(prefix+'control-object', ['clang.exe', *flags, '-c', str(case/'Control.c'), '-o', str(case/'control.obj')])
    run(prefix+'control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console', '/nodefaultlib',
                                '/out:'+str(case/'control.exe'), str(case/'control.obj'), str(case/'api.lib')])
    run(prefix+'control-run', [str(case/'control.exe')])
    llvm = run(prefix+'llvm', [args.nsc, '--dump-project-oop-llvm', str(case/'Probe.nsproj'), '--cpp-clang-root='+args.clang_root])
    if not re.search(r'call\s+x86_regcallcc\s+i32\s+@__regcall3__combine_mode\(i32 [^,]+, i32 3\)', llvm):
        raise SystemExit(prefix+'no se encontró la llamada regcall con el enum; evidencia: '+str(root))
    run(prefix+'ns-build', [args.nsc, '--emit-project-oop-exe', str(case/'Probe.nsproj'), '--cpp-clang-root='+args.clang_root,
                            '--no-artifact-cache', '-O'+str(opt), '-o', str(case/'out.exe')])
    run(prefix+'ns-run', [str(case/'out.exe')])
    print(prefix+'control C y NativeSharp correctos', flush=True)
case = root/'enum-cpp-no-admitido'
case.mkdir()
(case/'api.h').write_text('#pragma once\nnamespace T09N { enum class Mode : int { A = 1, B = 2 }; '
    'int __attribute__((regcall)) combine_mode(Mode mode, int bias); }\n', encoding='utf-8')
(case/'api.cpp').write_text('#include "api.h"\nnamespace T09N { int __attribute__((regcall)) combine_mode(Mode mode, int bias) '
    '{ return (int)mode * 10 + bias; } }\n', encoding='utf-8')
(case/'Main.ns').write_text('int Main() { return T09N.combine_mode(T09N.Mode.B, 3) == 23 ? 0 : 1; }\n', encoding='utf-8')
(case/'Negative.nsproj').write_text('<Project><PropertyGroup><Name>Negative</Name></PropertyGroup><ItemGroup>'
    '<Compile Include="Main.ns" /><CppLibraryReference Header="api.h" Language="C++" Standard="c++20" '
    'Library="api.lib" /></ItemGroup></Project>', encoding='utf-8')
run('enum-cpp-producer', ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-c', str(case/'api.cpp'), '-o', str(case/'api.obj')])
run('enum-cpp-archive', ['llvm-lib.exe', '/nologo', '/out:'+str(case/'api.lib'), str(case/'api.obj')])

# EN: T09-003. The former negative input, a C++ enum class by value with regcall, is now projected through Clang's
#     plan: the callee compiled by Clang returns the expected value and NSC calls it with x86_regcallcc.
# ES: T09-003. La entrada negativa anterior, un enum class C++ por valor con regcall, ya se proyecta por el plan de
#     Clang: el callee compilado por Clang devuelve el valor esperado y NSC lo llama con x86_regcallcc.
enumcpp_ir = subprocess.run([args.nsc, '--dump-project-oop-llvm', str(case/'Negative.nsproj'), '--cpp-clang-root=' + args.clang_root],
                             cwd=case, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
(root / 'enumcpp-ir.log').write_text('EXIT: ' + str(enumcpp_ir.returncode) + '\n' + enumcpp_ir.stdout, encoding='utf-8')
if enumcpp_ir.returncode or not re.search(r'(?:call|invoke) x86_regcallcc i32 @"\?combine_mode@', enumcpp_ir.stdout):
    raise SystemExit('El enum C++ con regcall no se llama con x86_regcallcc; evidencia: ' + str(root))
for enumcpp_opt in range(4):
    enumcpp_exe = str(case / ('native-enum-cpp.exe'.replace('.exe', '-O' + str(enumcpp_opt) + '.exe')))
    enumcpp_build = subprocess.run([args.nsc, '--emit-project-oop-exe', str(case/'Negative.nsproj'), '--cpp-clang-root=' + args.clang_root,
                                     '--no-artifact-cache', '-O' + str(enumcpp_opt), '-o', enumcpp_exe],
                                    cwd=case, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
    (root / ('enumcpp-build-O' + str(enumcpp_opt) + '.log')).write_text(enumcpp_build.stdout, encoding='utf-8')
    if enumcpp_build.returncode or subprocess.run([enumcpp_exe], cwd=case, env=env).returncode:
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
print('OK: forma ABI distinta, enum C y enum C++ comparados con Clang; union solo float comparada con Clang; struct C++ con campo de ancho cero rechazado', flush=True)
