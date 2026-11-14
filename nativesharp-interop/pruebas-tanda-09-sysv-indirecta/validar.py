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
(root/'BadUnion.h').write_text('#pragma once\ntypedef union T09UnsupportedUnion { int values[6]; double wide; } T09UnsupportedUnion;\n'
    'typedef int (__attribute__((sysv_abi)) *T09UnsupportedUnionCallback)(T09UnsupportedUnion);\n'
    'T09UnsupportedUnionCallback t09_get_unsupported_union_callback(void);\n', encoding='utf-8')
(root/'BadUnion.ns').write_text('int Main() {\n    T09UnsupportedUnionCallback callback = t09_get_unsupported_union_callback();\n'
    '    T09UnsupportedUnion value; value.wide = 1.0;\n    return callback(value) == 1 ? 0 : 1;\n}\n', encoding='utf-8')
(root/'BadUnion.nsproj').write_text((root/'Bad.nsproj').read_text(encoding='utf-8').replace('Bad.h', 'BadUnion.h')
    .replace('Bad.ns', 'BadUnion.ns'), encoding='utf-8')
command = [args.nsc, '--dump-project-oop-llvm', str(root/'BadUnion.nsproj'),
           '--cpp-clang-root=' + args.clang_root, '--library-reference-root', str(root)]
result = subprocess.run(command, cwd=repo, env=env, stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT, text=True, errors='replace')
(root/'rechazo-firma.log').write_text('COMMAND: ' + repr(command) + '\nEXIT: ' +
    str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
if result.returncode == 0 or 'sysv_abi function pointer byval plan is not supported' not in result.stdout:
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
