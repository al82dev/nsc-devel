"""Contrasta tamaños ocultos en métodos estáticos C++ con Clang."""
import argparse
import os
import re
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
parser.add_argument('--precompilado', action='store_true')
parser.add_argument('--no-generico', action='store_true')
args = parser.parse_args()
source = Path(__file__).resolve().parent
root = Path(tempfile.mkdtemp(prefix='nsc-t09-size-static-method-', dir='A:/temp'))
fixture = root / 'fixture'
shutil.copytree(source, fixture)

def replace(name, old, new):
    path = fixture / name
    text = path.read_text(encoding='utf-8')
    if text.count(old) != 1:
        raise SystemExit('control no encontrado: ' + name)
    path.write_text(text.replace(old, new), encoding='utf-8')

declarations, definitions, ns_tests, cpp_tests = [], [], [], []
for dynamic in (False, True):
    for mode in range(4):
        name = ('dynamic' if dynamic else 'size') + str(mode)
        attribute = 'pass_dynamic_object_size' if dynamic else 'pass_object_size'
        builtin = '__builtin_dynamic_object_size' if dynamic else '__builtin_object_size'
        declarations.append(f'    static T09_STORAGE int {name}(int *p __attribute__(({attribute}({mode}))));')
        definitions.append(f'int T09ObjectSize::Methods::{name}(int *const p __attribute__(({attribute}({mode})))) '
                           f'{{ return static_cast<int>({builtin}(p, {mode})); }}')
        ns_tests.append(f'    if (T09ObjectSize.Methods.{name}(&value) != 4) return 40;')
        cpp_tests.append(f'    if (T09ObjectSize::Methods::{name}(&value) != 4) return 40;')
declarations.append('    static T09_STORAGE int mixed(int *p __attribute__((pass_object_size(0))), bool value);')
declarations.append('    static T09_STORAGE int plain(int *p);')
definitions.append('int T09ObjectSize::Methods::mixed(int *const p __attribute__((pass_object_size(0))), bool value) '
                   '{ return static_cast<int>(__builtin_object_size(p, 0)) + value; }')
definitions.append('int T09ObjectSize::Methods::plain(int *p) { return *p; }')
ns_tests.append('    if (T09ObjectSize.Methods.mixed(&value,true) != 5) return 41;')
cpp_tests.append('    if (T09ObjectSize::Methods::mixed(&value,true) != 5) return 41;')
for name in ('size1', 'size3', 'dynamic1', 'dynamic3'):
    ns_tests.append(f'    if (T09ObjectSize.Methods.{name}(&array.values[constantIndex]) != 4) return 42;')
    cpp_tests.append(f'    if (T09ObjectSize::Methods::{name}(&array.values[constantIndex]) != 4) return 42;')
replace('api.h', 'T09_STORAGE int calls();', 'struct Methods {\n' + '\n'.join(declarations) + '\n};\nT09_STORAGE int calls();')
path = fixture / 'api.cpp'
path.write_text(path.read_text(encoding='utf-8') + '\n' + '\n'.join(definitions) + '\n', encoding='utf-8')
for name in ('Main.ns', 'Producer.ns'):
    replace(name, '    return 0;', '\n'.join(ns_tests) + '\n    return 0;')
replace('Control.cpp', '    return T09ObjectSize::size0', '\n'.join(cpp_tests) + '\n    return T09ObjectSize::size0')
replace('Pointer.ns', 'T09ObjectSize.Callback callback = T09ObjectSize.size0;',
        'T09ObjectSize.Callback callback = T09ObjectSize.Methods.size0;')
(fixture / 'PointerAllowed.ns').write_text('int Main() { T09ObjectSize.Callback callback = '
    'T09ObjectSize.Methods.plain; int value = 7; return callback(&value) == 7 ? 0 : 1; }\n', encoding='utf-8')
if args.no_generico:
    replace('Producer.ns', 'public static int Run<T>(T unused)', 'public static int Run(int unused)')
    replace('Consumer.ns', 'T09.SizeBridge.Run<int>(0)', 'T09.SizeBridge.Run(0)')
path = fixture / 'validar.py'
text = path.read_text(encoding='utf-8')
start = text.index('    if not args.solo_extremos:\n        for name in ')
end = text.index('    if opt == 0:', start)
path.write_text(text[:start] + text[end:], encoding='utf-8')
replace('validar.py', "print('OK: cuatro comparaciones; evidencia:', root)",
        "shutil.copyfile(source / 'PointerAllowed.ns', case / 'Main.ns')\n"
        "run('puntero-permitido-build', [args.nsc, '--emit-project-oop-exe', *common, '--no-artifact-cache', "
        "'-O3', '-o', str(case / 'pointer-allowed.exe')], case)\n"
        "run('puntero-permitido-run', [str(case / 'pointer-allowed.exe')], case)\n"
        "print('OK: cuatro comparaciones; evidencia:', root)")
# Exercise both the live declaration and the method restored for the template.
bridge_call = 'Run(0)' if args.no_generico else 'Run<int>(0)'
replace('validar-precompilado.py',
        "        shutil.copyfile(source / 'Consumer.ns', consumer / 'Consumer.ns')",
        "        shutil.copyfile(source / 'Consumer.ns', consumer / 'Consumer.ns')\n"
        "        if visibility == 'con-encabezado':\n"
        "            (consumer / 'Consumer.ns').write_text('int Main() { int value = 7; "
        "if (T09ObjectSize.Methods.size0(&value) != 4) return 91; "
        "return T09.SizeBridge." + bridge_call + "; }\\n', encoding='utf-8')")
scripts = ('validar.py', 'validar-precompilado.py') if args.precompilado else ('validar.py',)
for script in scripts:
    for dll in (False, True):
        command = [sys.executable, str(fixture / script), '--nsc', args.nsc,
                   '--clang-root', args.clang_root, '--const-parametros']
        if dll:
            command.append('--dll')
        result = subprocess.run(command, cwd=fixture, env=os.environ.copy(), stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, text=True, errors='replace')
        label = script.replace('.py', '') + ('-dll' if dll else '-estatica')
        (root / (label + '.log')).write_text('COMMAND: ' + repr(command) + '\nEXIT: '
            + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
        print(label, result.stdout, flush=True)
        if result.returncode:
            raise SystemExit('fallo; evidencia: ' + str(root))
        if script == 'validar-precompilado.py' and not dll and not args.no_generico:
            match = re.search(r'Evidencia:\s*(.+)', result.stdout)
            if not match:
                raise SystemExit('falta evidencia del consumidor: ' + str(root))
            conflict = root / 'biblioteca-distinta'
            shutil.copytree(Path(match.group(1).strip()) / 'O0', conflict)
            producer = conflict / 'producer'
            consumer = conflict / 'con-encabezado'
            shutil.copyfile(producer / 'api.h.oculto', producer / 'api.h')
            (producer / 'extra.cpp').write_text('extern "C" int t09_extra() { return 12; }\n', encoding='utf-8')
            commands = [
                ['clang-cl.exe', '/nologo', '/c', str(producer / 'extra.cpp'), '/Fo' + str(producer / 'extra.obj')],
                ['llvm-lib.exe', '/nologo', '/out:' + str(producer / 'api-other.lib'),
                 str(producer / 'api.obj'), str(producer / 'extra.obj')],
            ]
            for command in commands:
                control = subprocess.run(command, cwd=conflict, stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT, text=True, errors='replace')
                if control.returncode:
                    raise SystemExit('fallo al preparar biblioteca distinta: ' + control.stdout)
            project = consumer / 'Consumer.nsproj'
            project.write_text(project.read_text(encoding='utf-8').replace(
                'Library="../producer/api.lib"', 'Library="../producer/api-other.lib"'), encoding='utf-8')
            command = [args.nsc, '--emit-project-oop-exe', 'Consumer.nsproj',
                '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O0', '-o', 'other.exe']
            control = subprocess.run(command, cwd=consumer, env=os.environ.copy(), stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT, text=True, errors='replace')
            (conflict / 'resultado.log').write_text('COMMAND: ' + repr(command) + '\nEXIT: '
                + str(control.returncode) + '\n' + control.stdout, encoding='utf-8')
            if control.returncode == 0 or 'shared C++ static method library differs' not in control.stdout:
                raise SystemExit('no se rechazo la biblioteca distinta: ' + str(conflict))
            print('biblioteca distinta: rechazo correcto; evidencia:', conflict, flush=True)
        if script == 'validar-precompilado.py' and not args.no_generico:
            match = re.search(r'Evidencia:\s*(.+)', result.stdout)
            if not match:
                raise SystemExit('falta evidencia del consumidor: ' + str(root))
            conflict = root / ('dll-distinta' if dll else 'firma-distinta')
            shutil.copytree(Path(match.group(1).strip()) / 'O0', conflict)
            producer = conflict / 'producer'
            consumer = conflict / 'con-encabezado'
            shutil.copyfile(producer / 'api.h.oculto', producer / 'api.h')
            if dll:
                other = conflict / 'otra'
                other.mkdir()
                shutil.copyfile(producer / 'api.dll', other / 'api.dll')
                with (other / 'api.dll').open('ab') as out:
                    out.write(b'NSC-different-library-control')
                project = consumer / 'Consumer.nsproj'
                content = project.read_text(encoding='utf-8')
                original = 'Runtime="../producer/api.dll"'
                if content.count(original) != 1:
                    raise SystemExit('referencia DLL ausente: ' + str(project))
                project.write_text(content.replace(original, 'Runtime="../otra/api.dll"'), encoding='utf-8')
                expected = 'shared C++ static method library differs'
            else:
                header = producer / 'api.h'
                content = header.read_text(encoding='utf-8')
                original = 'static T09_STORAGE int size0(int *const p __attribute__((pass_object_size(0))));'
                if content.count(original) != 1:
                    raise SystemExit('declaracion ausente: ' + str(header))
                header.write_text(content.replace(original, original[:-1] + ' noexcept;'), encoding='utf-8')
                expected = 'shared C++ static method declaration differs'
            command = [args.nsc, '--emit-project-oop-exe', 'Consumer.nsproj',
                '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O0', '-o', 'other.exe']
            control = subprocess.run(command, cwd=consumer, env=os.environ.copy(), stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT, text=True, errors='replace')
            (conflict / 'resultado.log').write_text('COMMAND: ' + repr(command) + '\nEXIT: '
                + str(control.returncode) + '\n' + control.stdout, encoding='utf-8')
            if control.returncode == 0 or expected not in control.stdout:
                raise SystemExit('no se rechazo la discrepancia: ' + str(conflict))
            print(conflict.name + ': rechazo correcto; evidencia:', conflict, flush=True)
        if script == 'validar.py':
            match = re.search(r'Evidencia:\s*(.+)', result.stdout)
            if not match:
                raise SystemExit('falta evidencia de IR: ' + str(root))
            direct = Path(match.group(1).strip())
            for opt in range(4):
                ir = (direct / ('O' + str(opt) + '-ir.log')).read_text(encoding='utf-8')
                lines = [line for line in ir.splitlines() if 'mixed@Methods@T09ObjectSize' in line]
                if len(lines) != 2 or not any('(ptr, i64, i1 zeroext)' in line for line in lines) or not all('i1 zeroext' in line for line in lines):
                    raise SystemExit('firma física incorrecta: ' + str(direct))
print('OK: métodos estáticos con tamaño oculto; evidencia:', root)
