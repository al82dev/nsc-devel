"""Contrasta pass_object_size con Clang y llamadas NativeSharp reales."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
parser.add_argument('--solo-extremos', action='store_true', help='Comprueba solo los modos 0 y 2')
parser.add_argument('--const-parametros', action='store_true', help='Declara los punteros del encabezado como const')
args = parser.parse_args()
source = Path(__file__).resolve().parent
root = Path(tempfile.mkdtemp(prefix='nsc-t09-object-size-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)

def run(label, command, cwd):
    result = subprocess.run(command, cwd=cwd, env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    log = root / (label + '.log')
    log.write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode)
                   + '\n' + result.stdout, encoding='utf-8')
    if result.returncode:
        print(result.stdout, flush=True)
        raise SystemExit(label + ': fallo; evidencia: ' + str(log))
    return result.stdout

version = run('clang-version', ['clang.exe', '--version'], source)
if 'clang version 23.1.0' not in version:
    raise SystemExit('Se requiere Clang 23.1.0')

for opt in range(4):
    case = root / ('O' + str(opt))
    case.mkdir()
    for name in ('api.h', 'api.cpp', 'Control.cpp', 'Main.ns', 'Fixture.nsproj'):
        shutil.copyfile(source / name, case / name)
    if args.const_parametros:
        header = case / 'api.h'
        header.write_text(header.read_text(encoding='utf-8').replace(
            'int *p __attribute__', 'int *const p __attribute__').replace(
            'int *q __attribute__', 'int *const q __attribute__'), encoding='utf-8')
    if args.solo_extremos:
        main = case / 'Main.ns'
        main.write_text(main.read_text(encoding='utf-8').replace(
            '    if (T09ObjectSize.size1(&value) != 4) return 2;\n', '').replace(
            '    if (T09ObjectSize.size3(&value) != 4) return 4;\n', '').replace(
            '    if (T09ObjectSize.dynamic1(&value) != 4) return 17;\n', '').replace(
            '    if (T09ObjectSize.dynamic3(&value) != 4) return 18;\n', '').replace(
            '    if (T09ObjectSize.size1(T09ObjectSize.next()) != -1) return 13;\n', '').replace(
            '    if (T09ObjectSize.size3(T09ObjectSize.next()) != 0) return 14;\n', '').replace(
            'T09ObjectSize.calls() != 4', 'T09ObjectSize.calls() != 2').replace(
            '    if (T09ObjectSize.size1(&pair.first) != 4) return 15;\n', '').replace(
            '    if (T09ObjectSize.size3(&pair.first) != 4) return 16;\n', '').replace(
            '    if (T09ObjectSize.dynamic1(&pair.first) != 4) return 19;\n', '').replace(
            '    if (T09ObjectSize.dynamic3(&pair.first) != 4) return 20;\n', ''), encoding='utf-8')
        lines = main.read_text(encoding='utf-8').splitlines(keepends=True)
        main.write_text(''.join(line for line in lines if '&array.values[' not in line), encoding='utf-8')
        control = case / 'Control.cpp'
        control.write_text(control.read_text(encoding='utf-8').replace(
            'T09ObjectSize::size1(&value) != 4 ||', '').replace(
            ' || T09ObjectSize::size3(&value) != 4', '').replace(
            '           T09ObjectSize::dynamic1(&value) != 4 || T09ObjectSize::dynamic3(&value) != 4 ||\n', '').replace(
            '           T09ObjectSize::size1(T09ObjectSize::next()) != -1 ||\n', '').replace(
            '           T09ObjectSize::size3(T09ObjectSize::next()) != 0 || ', '           ').replace(
            'T09ObjectSize::calls() != 4', 'T09ObjectSize::calls() != 2').replace(
            ' ||\n           T09ObjectSize::size1(&pair.first) != 4 || T09ObjectSize::size3(&pair.first) != 4', '').replace(
            ' ||\n           T09ObjectSize::dynamic1(&pair.first) != 4 || T09ObjectSize::dynamic3(&pair.first) != 4', ''), encoding='utf-8')
        text = control.read_text(encoding='utf-8')
        text = text[:text.index(' ||\n           T09ObjectSize::size1(&array.values[0])')] + ';\n}\n'
        control.write_text(text, encoding='utf-8')
    prefix = 'O' + str(opt)
    flags = ['-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-O' + str(opt)]
    run(prefix + '-api', ['clang.exe', *flags, '-c', 'api.cpp', '-o', 'api.obj'], case)
    run(prefix + '-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], case)
    run(prefix + '-control-obj', ['clang.exe', *flags, '-c', 'Control.cpp', '-o', 'control.obj'], case)
    run(prefix + '-control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console',
                                   '/nodefaultlib', '/out:control.exe', 'control.obj', 'api.lib'], case)
    run(prefix + '-control-run', [str(case / 'control.exe')], case)
    common = ['Fixture.nsproj', '--library-reference-root', str(case),
              '--cpp-clang-root=' + args.clang_root]
    run(prefix + '-ir', [args.nsc, '--dump-project-oop-llvm', *common], case)
    run(prefix + '-build', [args.nsc, '--emit-project-oop-exe', *common, '--no-artifact-cache',
                            '-O' + str(opt), '-o', str(case / 'native.exe')], case)
    run(prefix + '-native-run', [str(case / 'native.exe')], case)
    print(prefix + ': control C++ y NativeSharp terminaron con codigo 0', flush=True)
    if not args.solo_extremos:
        for name in ('size0', 'size1', 'size2', 'size3', 'dynamic0', 'dynamic1', 'dynamic2', 'dynamic3'):
            (case / 'Variable.cpp').write_text('#include "api.h"\nint main() { T09ObjectSize::ArrayPair a; '
                'return T09ObjectSize::' + name + '(&a.values[T09ObjectSize::index()]); }\n', encoding='utf-8')
            (case / 'Main.ns').write_text('int Main() { T09ObjectSize.ArrayPair a; '
                'return T09ObjectSize.' + name + '(&a.values[T09ObjectSize.index()]); }\n', encoding='utf-8')
            label = prefix + '-variable-' + name
            run(label + '-control-obj', ['clang.exe', *flags, '-c', 'Variable.cpp', '-o', 'variable.obj'], case)
            run(label + '-control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console',
                                          '/nodefaultlib', '/out:variable.exe', 'variable.obj', 'api.lib'], case)
            run(label + '-build', [args.nsc, '--emit-project-oop-exe', *common, '--no-artifact-cache',
                                   '-O' + str(opt), '-o', str(case / 'variable-ns.exe')], case)
            outcomes = []
            for executable in ('variable.exe', 'variable-ns.exe'):
                result = subprocess.run([str(case / executable)], cwd=case, env=env,
                                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                outcomes.append(result.returncode)
            (root / (label + '-run.log')).write_text('Clang: ' + str(outcomes[0]) + '\nNativeSharp: '
                                                    + str(outcomes[1]) + '\n', encoding='utf-8')
            if outcomes[0] != outcomes[1]:
                raise SystemExit(label + ': resultados diferentes; evidencia: ' + str(root))
    if opt == 0:
        if not args.solo_extremos:
            shutil.copyfile(source / 'ConstIndex.ns', case / 'Main.ns')
            run(prefix + '-indice-const-build', [args.nsc, '--emit-project-oop-exe', *common,
                '--no-artifact-cache', '-O0', '-o', str(case / 'indice-const.exe')], case)
            run(prefix + '-indice-const-run', [str(case / 'indice-const.exe')], case)
        shutil.copyfile(source / 'Pointer.ns', case / 'Main.ns')
        command = [args.nsc, '--dump-project-oop-llvm', *common]
        result = subprocess.run(command, cwd=case, env=env, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, text=True, errors='replace')
        log = root / (prefix + '-puntero-rechazado.log')
        log.write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode)
                       + '\n' + result.stdout, encoding='utf-8')
        if result.returncode == 0 or 'pass_object_size cannot be converted' not in result.stdout:
            raise SystemExit('Conversion a puntero no rechazada correctamente; evidencia: ' + str(log))
print('OK: cuatro comparaciones; evidencia:', root)
