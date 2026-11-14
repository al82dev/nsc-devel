"""T09-003: contrasta una llamada indirecta SysV con el productor y Clang."""
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
repo = fixture.parents[1]
root = Path(tempfile.mkdtemp(prefix='nsc-t09-sysv-indirecta-', dir=r'A:\temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)

def run(label, command):
    result = subprocess.run(command, cwd=repo, env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    (root / (label + '.log')).write_text('COMMAND: ' + repr(command) + '\nEXIT: ' +
        str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
    if result.returncode:
        print(result.stdout, flush=True)
        raise SystemExit(label + ': fallo; evidencia: ' + str(root))
    return result.stdout

if 'clang version 23.1.0' not in run('version', ['clang.exe', '--version']):
    raise SystemExit('Se requiere Clang 23.1.0')
for name in ('api.h', 'api.c', 'variadic.s', 'Control.c', 'Main.ns', 'Static.nsproj', 'Bad.h', 'Bad.ns', 'Bad.nsproj',
             'BadConversion.ns', 'BadConversion.nsproj', 'BadVar.h', 'BadVar.ns', 'BadVar.nsproj'):
    shutil.copyfile(fixture / name, root / name)
flags = ['-target', 'x86_64-pc-windows-msvc', '-std=c11', '-O0']
run('productor', ['clang.exe', *flags, '-c', str(root/'api.c'), '-o', str(root/'api.obj')])
run('variadic-productor', ['clang.exe', *flags, '-c', str(root/'variadic.s'), '-o', str(root/'variadic.obj')])
run('biblioteca', ['llvm-lib.exe', '/nologo', '/out:' + str(root/'api.lib'), str(root/'api.obj'), str(root/'variadic.obj')])
run('control-ir', ['clang.exe', *flags, '-S', '-emit-llvm', str(root/'Control.c'), '-o', str(root/'control.ll')])
ir = (root/'control.ll').read_text(encoding='utf-8')
for result_type in ('i32', 'i64', 'double', 'float', 'ptr', 'void'):
    if not re.search(r'call\s+x86_64_sysvcc\s+'+result_type+r'\s+%[^\(]+\(', ir):
        raise SystemExit('Clang no emitio la convencion SysV indirecta para '+result_type+'; evidencia: '+str(root))
for bits, extension in ((8, 'signext'), (8, 'zeroext'), (16, 'signext'), (16, 'zeroext'), (1, 'zeroext')):
    pattern = r'call\s+x86_64_sysvcc\s+'+extension+r'\s+i'+str(bits)+r'\s+%[^\(]+\(i'+str(bits)+r'\s+(?:noundef\s+)?'+extension
    if not re.search(pattern, ir):
        raise SystemExit('Clang no emitio '+extension+' i'+str(bits)+'; evidencia: '+str(root))
variadic_pattern = r'call\s+x86_64_sysvcc\s+i32\s+\(i32,\s*\.\.\.\)\s+%[^\(]+\(i32[^,]*,\s*i32[^,]*,\s*double[^,]*,\s*ptr\b'
if not re.search(variadic_pattern, ir):
    raise SystemExit('Clang no emitio las promociones variadicas esperadas; evidencia: '+str(root))
run('control-obj', ['clang.exe', *flags, '-c', str(root/'Control.c'), '-o', str(root/'control.obj')])
run('control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console', '/nodefaultlib',
    '/out:' + str(root/'control.exe'), str(root/'control.obj'), str(root/'api.lib')])
run('control-run', [str(root/'control.exe')])
print('Control C y convencion indirecta de Clang: OK', flush=True)
common = [str(root/'Static.nsproj'), '--cpp-clang-root=' + args.clang_root, '--library-reference-root', str(root)]
ns_ir = run('ns-ir', [args.nsc, '--dump-project-oop-llvm', *common])
if not re.search(variadic_pattern, ns_ir):
    raise SystemExit('NativeSharp no emitio las promociones variadicas esperadas; evidencia: '+str(root))
for bits, extension in ((8, 'signext'), (8, 'zeroext'), (16, 'signext'), (16, 'zeroext'), (1, 'zeroext')):
    pattern = r'call\s+x86_64_sysvcc\s+'+extension+r'\s+i'+str(bits)+r'\s+%[^\(]+\(i'+str(bits)+r'\s+(?:noundef\s+)?'+extension
    if not re.search(pattern, ns_ir):
        raise SystemExit('NativeSharp no emitio '+extension+' i'+str(bits)+'; evidencia: '+str(root))
if not re.search(r'call\s+x86_64_sysvcc\s+i32\s+%[^\(]+\(i32', ns_ir):
    raise SystemExit('NativeSharp no emitio la convencion SysV indirecta; evidencia: ' + str(root))
run('ns-build', [args.nsc, '--emit-project-oop-exe', *common, '--no-artifact-cache', '-O0', '-o', str(root/'ns.exe')])
run('ns-run', [str(root/'ns.exe')])
# EN: T09-003. The former rejected signature, a 24-byte C struct passed byval through a sysv_abi pointer, is
#     lowered through the Clang plan of its alias: it is now compared against Clang. The byval rejection
#     check is kept, unchanged, with a C union of the same size, which NSC does not yet lower by value.
# ES: T09-003. La firma antes rechazada (struct C de 24 bytes byval por un puntero sysv_abi) se baja por el
#     plan de Clang de su alias y se compara ahora con Clang. El rechazo byval se conserva con una union C
#     del mismo tamano, que NSC aun no baja por valor.
byval = root / 'byval-24'
byval.mkdir()
shutil.copyfile(fixture / 'Bad.h', byval / 'Bad.h')
(byval / 'bad.c').write_text('#include "Bad.h"\n'
    'static int __attribute__((sysv_abi)) t09_sum_six(T09UnsupportedRecord r)\n'
    '{ int result = r.x + r.y * 2 + r.z * 3 + r.w * 4 + r.v * 5 + r.u * 6; r.x = 99; return result; }\n'
    'T09UnsupportedCallback t09_get_unsupported_callback(void) { return t09_sum_six; }\n', encoding='utf-8')
(byval / 'Control.c').write_text('#include "Bad.h"\n'
    'void *memcpy(void *d, const void *s, unsigned long long n)\n'
    '{ volatile unsigned char *o = d; const volatile unsigned char *i = s; for (unsigned long long k = 0; k < n; ++k) o[k] = i[k]; return d; }\n'
    'int main(void) {\n'
    '    T09UnsupportedRecord value = {1, 2, 3, 4, 5, 6};\n'
    '    return t09_get_unsupported_callback()(value) == 91 && value.x == 1 ? 0 : 1;\n}\n', encoding='utf-8')
(byval / 'Main.ns').write_text('int Main() {\n'
    '    T09UnsupportedCallback callback = t09_get_unsupported_callback();\n'
    '    T09UnsupportedRecord value; value.x = 1; value.y = 2; value.z = 3; value.w = 4; value.v = 5; value.u = 6;\n'
    '    if (callback(value) != 91 || value.x != 1 || value.u != 6) return 1;\n'
    '    return 0;\n}\n', encoding='utf-8')
(byval / 'Byval.nsproj').write_text('<Project><PropertyGroup><Name>T09Byval</Name></PropertyGroup><ItemGroup>'
    '<Compile Include="Main.ns" /><CppLibraryReference Header="Bad.h" Language="C" Standard="c11" '
    'Library="bad.lib" /></ItemGroup></Project>', encoding='utf-8')
run('byval-productor', ['clang.exe', *flags, '-c', str(byval/'bad.c'), '-o', str(byval/'bad.obj')])
run('byval-biblioteca', ['llvm-lib.exe', '/nologo', '/out:' + str(byval/'bad.lib'), str(byval/'bad.obj')])
byval_ir = run('byval-clang-ir', ['clang.exe', *flags, '-S', '-emit-llvm', str(byval/'Control.c'), '-o', '-'])
if not re.search(r'call\s+x86_64_sysvcc\s+i32\s+%[^\(]+\(ptr\s+(?:noundef\s+)?byval\(%struct\.T09UnsupportedRecord\)\s+align\s+8', byval_ir):
    raise SystemExit('Clang no paso el record de 24 bytes byval; evidencia: ' + str(root))
run('byval-control-obj', ['clang.exe', *flags, '-c', str(byval/'Control.c'), '-o', str(byval/'control.obj')])
run('byval-control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console', '/nodefaultlib',
    '/out:' + str(byval/'control.exe'), str(byval/'control.obj'), str(byval/'bad.lib')])
run('byval-control-run', [str(byval/'control.exe')])
byval_common = [str(byval/'Byval.nsproj'), '--cpp-clang-root=' + args.clang_root]
byval_ns_ir = run('byval-ns-ir', [args.nsc, '--dump-project-oop-llvm', *byval_common])
if not re.search(r'call\s+x86_64_sysvcc\s+i32\s+%[^\(]+\(ptr\s+byval\(\[24 x i8\]\)\s+align\s+8', byval_ns_ir) and \
        not re.search(r'call\s+x86_64_sysvcc\s+i32\s+%[^\(]+\(ptr\s+byval\([^)]*\)\s+align\s+8', byval_ns_ir):
    raise SystemExit('NativeSharp no paso el record de 24 bytes byval; evidencia: ' + str(root))
run('byval-ns-build', [args.nsc, '--emit-project-oop-exe', *byval_common, '--no-artifact-cache', '-O0',
    '-o', str(byval/'ns.exe')])
run('byval-ns-run', [str(byval/'ns.exe')])
print('Record de 24 bytes byval por puntero: control C y NativeSharp OK', flush=True)
# EN: T09-003. The C union of the same size, the former byval rejection, is lowered through the Clang plan of
#     its alias since C unions by value follow the C struct path: it is compared against Clang as well. The
#     rejection check is kept with a C callback taking __int128, which Clang compiles and NSC does not import;
#     the old message belonged to a path that C records no longer reach.
# ES: T09-003. La union C del mismo tamano, antes rechazo byval, se baja por el plan de Clang de su alias
#     desde que las uniones C por valor siguen la ruta de los structs C: tambien se compara con Clang. El
#     rechazo se conserva con un callback C que recibe __int128, que Clang compila y NSC no importa; el
#     mensaje anterior pertenecia a una ruta a la que ya no llegan los records C.
union24 = root / 'union-24'
union24.mkdir()
(union24/'BadUnion.h').write_text('#pragma once\ntypedef union T09UnsupportedUnion { int values[6]; double wide; } T09UnsupportedUnion;\n'
    'typedef int (__attribute__((sysv_abi)) *T09UnsupportedUnionCallback)(T09UnsupportedUnion);\n'
    'T09UnsupportedUnionCallback t09_get_unsupported_union_callback(void);\n', encoding='utf-8')
(union24/'bad.c').write_text('#include "BadUnion.h"\n'
    'static int __attribute__((sysv_abi)) t09_union_sum(T09UnsupportedUnion u)\n'
    '{ int result = u.values[0] + u.values[5] * 2; u.values[0] = 99; return result; }\n'
    'T09UnsupportedUnionCallback t09_get_unsupported_union_callback(void) { return t09_union_sum; }\n', encoding='utf-8')
(union24/'Control.c').write_text('#include "BadUnion.h"\n'
    'void *memcpy(void *d, const void *s, unsigned long long n)\n'
    '{ volatile unsigned char *o = d; const volatile unsigned char *i = s; for (unsigned long long k = 0; k < n; ++k) o[k] = i[k]; return d; }\n'
    'int main(void) {\n'
    '    T09UnsupportedUnion value = {{1, 2, 3, 4, 5, 6}};\n'
    '    return t09_get_unsupported_union_callback()(value) == 13 && value.values[0] == 1 ? 0 : 1;\n}\n', encoding='utf-8')
(union24/'Main.ns').write_text('int Main() {\n'
    '    T09UnsupportedUnionCallback callback = t09_get_unsupported_union_callback();\n'
    '    T09UnsupportedUnion value; value.values[0] = 1; value.values[5] = 6;\n'
    '    if (callback(value) != 13 || value.values[0] != 1 || value.values[5] != 6) return 1;\n'
    '    return 0;\n}\n', encoding='utf-8')
(union24/'Union.nsproj').write_text('<Project><PropertyGroup><Name>T09Union</Name></PropertyGroup><ItemGroup>'
    '<Compile Include="Main.ns" /><CppLibraryReference Header="BadUnion.h" Language="C" Standard="c11" '
    'Library="bad.lib" /></ItemGroup></Project>', encoding='utf-8')
run('union-productor', ['clang.exe', *flags, '-c', str(union24/'bad.c'), '-o', str(union24/'bad.obj')])
run('union-biblioteca', ['llvm-lib.exe', '/nologo', '/out:' + str(union24/'bad.lib'), str(union24/'bad.obj')])
union_ir = run('union-clang-ir', ['clang.exe', *flags, '-S', '-emit-llvm', str(union24/'Control.c'), '-o', '-'])
if not re.search(r'call\s+x86_64_sysvcc\s+i32\s+%[^\(]+\(ptr\s+(?:noundef\s+)?byval\(%union\.T09UnsupportedUnion\)\s+align\s+8', union_ir):
    raise SystemExit('Clang no paso la union de 24 bytes byval; evidencia: ' + str(root))
run('union-control-obj', ['clang.exe', *flags, '-c', str(union24/'Control.c'), '-o', str(union24/'control.obj')])
run('union-control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console', '/nodefaultlib',
    '/out:' + str(union24/'control.exe'), str(union24/'control.obj'), str(union24/'bad.lib')])
run('union-control-run', [str(union24/'control.exe')])
union_common = [str(union24/'Union.nsproj'), '--cpp-clang-root=' + args.clang_root]
union_ns_ir = run('union-ns-ir', [args.nsc, '--dump-project-oop-llvm', *union_common])
if not re.search(r'call\s+x86_64_sysvcc\s+i32\s+%[^\(]+\(ptr\s+byval\([^)]*\)\s+align\s+8', union_ns_ir):
    raise SystemExit('NativeSharp no paso la union de 24 bytes byval; evidencia: ' + str(root))
run('union-ns-build', [args.nsc, '--emit-project-oop-exe', *union_common, '--no-artifact-cache', '-O0',
    '-o', str(union24/'ns.exe')])
run('union-ns-run', [str(union24/'ns.exe')])
print('Union de 24 bytes byval por puntero: control C y NativeSharp OK', flush=True)
(root/'BadWide.h').write_text('#pragma once\n'
    'typedef int (__attribute__((sysv_abi)) *T09WideCallback)(__int128);\n'
    'T09WideCallback t09_get_wide_callback(void);\n', encoding='utf-8')
(root/'BadWide.ns').write_text('int Main() {\n    T09WideCallback callback = t09_get_wide_callback();\n'
    '    return callback(1) == 1 ? 0 : 1;\n}\n', encoding='utf-8')
(root/'BadWide.nsproj').write_text((root/'Bad.nsproj').read_text(encoding='utf-8').replace('Bad.h', 'BadWide.h')
    .replace('Bad.ns', 'BadWide.ns'), encoding='utf-8')
run('wide-clang', ['clang.exe', *flags, '-fsyntax-only', str(root/'BadWide.h')])
command = [args.nsc, '--dump-project-oop-llvm', str(root/'BadWide.nsproj'),
           '--cpp-clang-root=' + args.clang_root, '--library-reference-root', str(root)]
result = subprocess.run(command, cwd=repo, env=env, stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT, text=True, errors='replace')
(root/'rechazo-firma.log').write_text('COMMAND: ' + repr(command) + '\nEXIT: ' +
    str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
if result.returncode == 0 or 'C callback signature contains an unavailable type' not in result.stdout:
    raise SystemExit('La firma no verificada no fue rechazada correctamente; evidencia: ' + str(root))
command = [args.nsc, '--dump-project-oop-llvm', str(root/'BadConversion.nsproj'),
           '--cpp-clang-root=' + args.clang_root, '--library-reference-root', str(root)]
result = subprocess.run(command, cwd=repo, env=env, stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT, text=True, errors='replace')
(root/'rechazo-conversion.log').write_text('COMMAND: ' + repr(command) + '\nEXIT: ' +
    str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
if result.returncode == 0 or 'sysv_abi function pointer is not yet supported' not in result.stdout:
    raise SystemExit('La conversion incompatible no fue rechazada correctamente; evidencia: ' + str(root))
command = [args.nsc, '--dump-project-oop-llvm', str(root/'BadVar.nsproj'),
           '--cpp-clang-root=' + args.clang_root, '--library-reference-root', str(root)]
result = subprocess.run(command, cwd=repo, env=env, stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT, text=True, errors='replace')
(root/'rechazo-variadico-record.log').write_text('COMMAND: ' + repr(command) + '\nEXIT: ' +
    str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
if result.returncode == 0 or 'sysv_abi variadic function pointer byval plan is not supported' not in result.stdout:
    raise SystemExit('El record variadico no fue rechazado correctamente; evidencia: ' + str(root))
print('OK: llamada indirecta C/NativeSharp, estatica O0; evidencia:', root, flush=True)
