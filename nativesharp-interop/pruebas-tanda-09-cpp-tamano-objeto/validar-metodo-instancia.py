"""Compara tamaños ocultos en métodos C++ de instancia contra Clang."""
import argparse
import os
import shutil
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
parser.add_argument('--dll', action='store_true')
parser.add_argument('--precompilado', action='store_true')
args = parser.parse_args()
root = Path(tempfile.mkdtemp(prefix='nsc-t09-instance-object-size-', dir='A:/temp'))
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

if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version'], root):
    raise SystemExit('Se requiere Clang 23.1.0')

header = ['#pragma once', '#if defined(T09_EXPORT)', '#define API __declspec(dllexport)',
          '#elif defined(T09_IMPORT)', '#define API __declspec(dllimport)', '#else', '#define API', '#endif',
          'namespace T09Instance {', 'struct ArrayPair { int values[2]; };', 'struct Probe {', 'int marker;']
implementation = ['#include "api.h"', 'namespace T09Instance {', 'static Probe object{7};',
                  'static Other otherObject{30};', 'static Probe alternate{17};', 'Probe* make() { return &object; }',
                  'Probe* makeAlternate() { return &alternate; }',
                  'Probe* makeWith(int choice) { return choice ? &alternate : &object; }',
                  'Probe* makeOver(int) { return &alternate; }', 'Other* makeOver(long long) { return &otherObject; }',
                  'Probe* makeTie(int, long long) { return &alternate; }',
                  'Other* makeTie(long long, int) { return &otherObject; }',
                  'Probe* makeArity(int) { return &alternate; }',
                  'Other* makeArity(int, int) { return &otherObject; }',
                  'Probe* makeVar(int choice, ...) { __builtin_va_list args; __builtin_va_start(args, choice); '
                  'int value = __builtin_va_arg(args, int); __builtin_va_end(args); '
                  'return choice && value == 9 ? &alternate : &object; }',
                  'static int calls = 0;', 'int next() { ++calls; return 1; }', 'int callCount() { return calls; }',
                  'Other* makeOther() { return &otherObject; }',
                  'int Other::size0(int *const p __attribute__((pass_object_size(0))), bool b) const '
                  '{ return marker + static_cast<int>(__builtin_object_size(p, 0)) + b; }']
ns_tests, cpp_tests = [], []
for dynamic in (False, True):
    for mode in range(4):
        name = ('dynamic' if dynamic else 'size') + str(mode)
        attr = 'pass_dynamic_object_size' if dynamic else 'pass_object_size'
        builtin = '__builtin_dynamic_object_size' if dynamic else '__builtin_object_size'
        header.append(f'API int {name}(int *p __attribute__(({attr}({mode}))), bool b) const;')
        implementation.append(f'int Probe::{name}(int *const p __attribute__(({attr}({mode}))), bool b) const '
                              f'{{ return marker + static_cast<int>({builtin}(p, {mode})) + b; }}')
        ns_tests.append(f'if (object->{name}(&value, true) != 12) return {40 + mode};')
        cpp_tests.append(f'if (object->{name}(&value, true) != 12) return {40 + mode};')
for name in ('size1', 'size3', 'dynamic1', 'dynamic3'):
    ns_tests.append(f'if (object->{name}(&pair.values[constantIndex], true) != 12) return 50;')
    cpp_tests.append(f'if (object->{name}(&pair.values[constantIndex], true) != 12) return 50;')
ns_tests.append('if (other->size0(&value, true) != 35) return 55;')
cpp_tests.append('if (other->size0(&value, true) != 35) return 55;')
header.extend(['};', 'struct Other { int marker; API int size0(int *p '
    '__attribute__((pass_object_size(0))), bool b) const; };', 'API Probe* make();', 'API Probe* makeAlternate();', 'API Probe* makeWith(int choice);', 'API Probe* makeOver(int);', 'API Other* makeOver(long long);', 'API Probe* makeTie(int, long long);', 'API Other* makeTie(long long, int);', 'API Probe* makeArity(int);', 'API Other* makeArity(int, int);', 'API Probe* makeVar(int, ...);', 'API int next();', 'API int callCount();', 'API Other* makeOther();', '}'])
implementation.append('}')
for opt in range(4):
    case = root / ('O' + str(opt)); case.mkdir()
    (case / 'api.h').write_text(('\n'.join(header) + '\n'), encoding='utf-8')
    (case / 'api.cpp').write_text(('\n'.join(implementation) + '\n'), encoding='utf-8')
    (case / 'Control.cpp').write_text('#include "api.h"\nint main() { auto object = T09Instance::make(); '
        'auto other = T09Instance::makeOther(); int value = 3; T09Instance::ArrayPair pair; const int constantIndex = 1; '
        'object = T09Instance::makeAlternate(); if (object->size0(&value, true) != 22) return 56; '
        'object = T09Instance::makeWith(T09Instance::next()); if (object->size0(&value, true) != 22) return 57; '
        'if (T09Instance::callCount() != 1) return 58; '
        'object = T09Instance::makeOver(1); if (object->size0(&value, true) != 22) return 59; object = T09Instance::make(); '
        'if (T09Instance::makeArity(1)->size0(&value, true) != 22 || T09Instance::makeArity(1, 2)->size0(&value, true) != 35) return 60; '
        'if (T09Instance::makeVar(1, 9)->size0(&value, true) != 22) return 61; '
        'if (T09Instance::makeWith(1)->size0(&value, true) != 22) return 62; '
        'if (T09Instance::makeOver(1)->size0(&value, true) != 22 || '
        'T09Instance::makeOver(1LL)->size0(&value, true) != 35) return 63; '
        + ' '.join(cpp_tests) + ' return 0; }\n', encoding='utf-8')
    (case / 'Main.ns').write_text('int Main() { var object = T09Instance.make(); var other = T09Instance.makeOther(); int value = 3; T09Instance.ArrayPair pair; const int constantIndex = 1; '
        'object = T09Instance.makeAlternate(); if (object->size0(&value, true) != 22) return 56; '
        'object = T09Instance.makeWith(T09Instance.next()); if (object->size0(&value, true) != 22) return 57; '
        'if (T09Instance.callCount() != 1) return 58; '
        'object = T09Instance.makeOver(1); if (object->size0(&value, true) != 22) return 59; object = T09Instance.make(); '
        'if (T09Instance.makeArity(1)->size0(&value, true) != 22 || T09Instance.makeArity(1, 2)->size0(&value, true) != 35) return 60; '
        'if (T09Instance.makeVar(1, 9)->size0(&value, true) != 22) return 61; '
        'if (T09Instance.makeWith(1)->size0(&value, true) != 22) return 62; '
        'if (T09Instance.makeOver(1)->size0(&value, true) != 22 || '
        'T09Instance.makeOver((long)1)->size0(&value, true) != 35) return 63; '
        + ' '.join(ns_tests) + ' return 0; }\n', encoding='utf-8')
    runtime = ' Runtime="api.dll"' if args.dll else ''
    (case / 'Fixture.nsproj').write_text('<Project><PropertyGroup><Name>Fixture</Name></PropertyGroup><ItemGroup>'
        '<Compile Include="Main.ns" /><CppLibraryReference Header="api.h" Language="C++" Standard="c++20" '
        'Library="api.lib"' + runtime + ' /></ItemGroup></Project>', encoding='utf-8')
    flags = ['-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-O' + str(opt)]
    (case / 'Tie.cpp').write_text('#include "api.h"\nvoid f() { T09Instance::makeTie(1, 1); }\n', encoding='utf-8')
    tie_command = ['clang.exe', *flags, '-fsyntax-only', 'Tie.cpp']
    tie_result = subprocess.run(tie_command, cwd=case, env=env, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, text=True, errors='replace')
    (root / f'O{opt}-clang-tie.log').write_text('COMMAND: ' + repr(tie_command) + '\nEXIT: '
        + str(tie_result.returncode) + '\n' + tie_result.stdout, encoding='utf-8')
    if tie_result.returncode == 0 or 'ambiguous' not in tie_result.stdout.lower():
        raise SystemExit('Clang no rechazó la sobrecarga ambigua: ' + str(case))
    run(f'O{opt}-api', ['clang.exe', *flags, *(['-DT09_EXPORT'] if args.dll else []),
                         '-c', 'api.cpp', '-o', 'api.obj'], case)
    if args.dll:
        run(f'O{opt}-lib', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib',
                             '/out:api.dll', '/implib:api.lib', 'api.obj'], case)
        path = case / 'api.h'; path.write_text('#define T09_IMPORT 1\n' + path.read_text(encoding='utf-8'), encoding='utf-8')
    else:
        run(f'O{opt}-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], case)
    run(f'O{opt}-control-obj', ['clang.exe', *flags, '-c', 'Control.cpp', '-o', 'control.obj'], case)
    run(f'O{opt}-control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console',
                                  '/nodefaultlib', '/out:control.exe', 'control.obj', 'api.lib'], case)
    run(f'O{opt}-control-run', [str(case / 'control.exe')], case)
    common = ['Fixture.nsproj', '--cpp-clang-root=' + args.clang_root]
    ir = run(f'O{opt}-ir', [args.nsc, '--dump-project-oop-llvm', *common], case)
    lines = [line for line in ir.splitlines() if 'size0@Probe@T09Instance' in line]
    if len(lines) != 9 or not all('i1 zeroext' in line for line in lines):
        raise SystemExit(f'O{opt}: firma de instancia inesperada; evidencia: {root}')
    run(f'O{opt}-build', [args.nsc, '--emit-project-oop-exe', *common, '--no-artifact-cache',
                           '-O' + str(opt), '-o', 'native.exe'], case)
    run(f'O{opt}-native-run', [str(case / 'native.exe')], case)
    if args.precompilado:
        producer = case / 'producer'; producer.mkdir()
        for name in ('api.h', 'api.lib') + (('api.dll',) if args.dll else ()):
            shutil.copyfile(case / name, producer / name)
        (producer / 'Producer.ns').write_text('namespace T09 { public class Bridge { public static int Run<T>(T unused) { '
            'var object = T09Instance.make(); var other = T09Instance.makeOther(); int value = 3; T09Instance.ArrayPair pair; const int constantIndex = 1; '
            + ' '.join(ns_tests) + ' return 0; } } }\n', encoding='utf-8')
        (producer / 'Producer.nsproj').write_text('<Project><PropertyGroup><Name>T09Producer</Name><OutputType>Library</OutputType>'
            '</PropertyGroup><ItemGroup><Compile Include="Producer.ns" /><CppLibraryReference Header="api.h" '
            'Language="C++" Standard="c++20" Library="api.lib"' + runtime + ' /></ItemGroup></Project>', encoding='utf-8')
        run(f'O{opt}-producer', [args.nsc, '--emit-project-oop-lib', 'Producer.nsproj',
            '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O' + str(opt), '-o', 'Producer.lib'], producer)
        for visibility in ('con-encabezado', 'sin-encabezado'):
            consumer = case / visibility; consumer.mkdir()
            main = 'int Main() { return T09.Bridge.Run<int>(0); }\n'
            if visibility == 'con-encabezado':
                main = ('int Main() { var left = T09Instance.make(); var right = T09Instance.makeOther(); '
                    'int value = 3; if (left->size0(&value, true) != 12) return 71; '
                    'if (right->size0(&value, true) != 35) return 72; return T09.Bridge.Run<int>(0); }\n')
            (consumer / 'Main.ns').write_text(main, encoding='utf-8')
            references = '<LibraryReference Include="../producer/Producer.lib" />'
            if visibility == 'con-encabezado':
                references += ('<CppLibraryReference Header="../producer/api.h" Language="C++" Standard="c++20" '
                    'Library="../producer/api.lib"' + (' Runtime="../producer/api.dll"' if args.dll else '') + ' />')
            else:
                (producer / 'api.h').rename(producer / 'api.h.oculto')
            (consumer / 'Consumer.nsproj').write_text('<Project><PropertyGroup><Name>T09Consumer</Name></PropertyGroup>'
                '<ItemGroup><Compile Include="Main.ns" />' + references + '</ItemGroup></Project>', encoding='utf-8')
            run(f'O{opt}-{visibility}-build', [args.nsc, '--emit-project-oop-exe', 'Consumer.nsproj',
                '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O' + str(opt), '-o', 'consumer.exe'], consumer)
            run(f'O{opt}-{visibility}-run', [str(consumer / 'consumer.exe')], consumer)
    if args.precompilado and opt == 0:
        for kind in (('firma', 'dll') if args.dll else ('firma', 'biblioteca')):
            conflict = root / ('conflicto-' + kind)
            shutil.copytree(case, conflict)
            producer = conflict / 'producer'; consumer = conflict / 'con-encabezado'
            shutil.copyfile(producer / 'api.h.oculto', producer / 'api.h')
            project = consumer / 'Consumer.nsproj'
            if kind == 'firma':
                path = producer / 'api.h'; content = path.read_text(encoding='utf-8')
                original = '\nAPI int size0(int *p __attribute__((pass_object_size(0))), bool b) const;'
                if content.count(original) != 1: raise SystemExit('declaración de control ausente')
                path.write_text(content.replace(original, original[:-1] + ' noexcept;'), encoding='utf-8')
                expected = 'shared C++ instance method declaration differs'
            elif kind == 'dll':
                other = conflict / 'otra'; other.mkdir()
                shutil.copyfile(producer / 'api.dll', other / 'api.dll')
                with (other / 'api.dll').open('ab') as out: out.write(b'NSC-different-dll-control')
                content = project.read_text(encoding='utf-8')
                old = 'Runtime="../producer/api.dll"'
                if content.count(old) != 1: raise SystemExit('referencia DLL ausente')
                project.write_text(content.replace(old, 'Runtime="../otra/api.dll"'), encoding='utf-8')
                expected = 'shared C++ instance method library differs'
            else:
                (producer / 'extra.cpp').write_text('extern "C" int t09_extra() { return 12; }\n', encoding='utf-8')
                run('conflicto-extra', ['clang-cl.exe', '/nologo', '/c', 'extra.cpp', '/Foextra.obj'], producer)
                run('conflicto-biblioteca', ['llvm-lib.exe', '/nologo', '/out:api-other.lib',
                    str(conflict / 'api.obj'), 'extra.obj'], producer)
                content = project.read_text(encoding='utf-8')
                old = 'Library="../producer/api.lib"'
                if content.count(old) != 1: raise SystemExit('referencia .lib ausente')
                project.write_text(content.replace(old, 'Library="../producer/api-other.lib"'), encoding='utf-8')
                expected = 'shared C++ instance method library differs'
            command = [args.nsc, '--emit-project-oop-exe', 'Consumer.nsproj', '--cpp-clang-root=' + args.clang_root,
                '--no-artifact-cache', '-O0', '-o', 'other.exe']
            control = subprocess.run(command, cwd=consumer, env=env, stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT, text=True, errors='replace')
            (conflict / 'resultado.log').write_text('COMMAND: ' + repr(command) + '\nEXIT: '
                + str(control.returncode) + '\n' + control.stdout, encoding='utf-8')
            if control.returncode == 0 or expected not in control.stdout:
                raise SystemExit('conflicto ' + kind + ' no rechazado; evidencia: ' + str(conflict))
            print('conflicto ' + kind + ': rechazo correcto', flush=True)
    if args.precompilado:
        for variant, factory in (('misma-fabrica', 'make'), ('otra-fabrica', 'makeAlternate'), ('no-resuelto', None)):
            conflict = root / ('O' + str(opt) + '-receptor-' + variant); shutil.copytree(case, conflict)
            producer = conflict / 'producer'
            shutil.copyfile(producer / 'api.h.oculto', producer / 'api.h')
            path = producer / 'Producer.ns'; content = path.read_text(encoding='utf-8')
            original = 'var object = T09Instance.make();'
            if content.count(original) != 1: raise SystemExit('inicializador del receptor ausente')
            assigned = 'T09Instance.' + factory + '()' if factory else 'null'
            content = content.replace(original, original + ' object = ' + assigned + ';')
            if variant == 'otra-fabrica': content = content.replace(' != 12)', ' != 22)')
            path.write_text(content, encoding='utf-8')
            run('O' + str(opt) + '-receptor-' + variant + '-producer', [args.nsc, '--emit-project-oop-lib', 'Producer.nsproj',
                '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O' + str(opt), '-o', 'Producer-new.lib'], producer)
            for visibility in ('con-encabezado', 'sin-encabezado'):
                consumer = conflict / visibility
                if visibility == 'sin-encabezado':
                    (producer / 'api.h').rename(producer / 'api.h.oculto2')
                project = consumer / 'Consumer.nsproj'; content = project.read_text(encoding='utf-8')
                original = 'Include="../producer/Producer.lib"'
                if content.count(original) != 1: raise SystemExit('referencia de productor ausente')
                project.write_text(content.replace(original, 'Include="../producer/Producer-new.lib"'), encoding='utf-8')
                command = [args.nsc, '--emit-project-oop-exe', 'Consumer.nsproj', '--cpp-clang-root=' + args.clang_root,
                    '--no-artifact-cache', '-O' + str(opt), '-o', 'other.exe']
                control = subprocess.run(command, cwd=consumer, env=env, stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT, text=True, errors='replace')
                (conflict / ('resultado-' + visibility + '.log')).write_text('COMMAND: ' + repr(command) + '\nEXIT: '
                    + str(control.returncode) + '\n' + control.stdout, encoding='utf-8')
                if factory:
                    if control.returncode != 0:
                        raise SystemExit('receptor reasignado no aceptado; evidencia: ' + str(conflict))
                    run('O' + str(opt) + '-receptor-' + variant + '-' + visibility + '-run', [str(consumer / 'other.exe')], consumer)
                    print('receptor ' + variant + ' ' + visibility + ': ejecución correcta', flush=True)
                else:
                    if control.returncode == 0 or "no compatible member named 'size0'" not in control.stdout:
                        raise SystemExit('receptor no resuelto no rechazado; evidencia: ' + str(conflict))
                    print('receptor no resuelto ' + visibility + ': rechazo correcto', flush=True)
    if args.precompilado:
        for variant in ('inicializador', 'asignacion', 'efecto'):
            conflict = root / ('O' + str(opt) + '-argumento-' + variant); shutil.copytree(case, conflict)
            producer = conflict / 'producer'; shutil.copyfile(producer / 'api.h.oculto', producer / 'api.h')
            path = producer / 'Producer.ns'; content = path.read_text(encoding='utf-8')
            original = 'var object = T09Instance.make();'
            if content.count(original) != 1: raise SystemExit('inicializador ausente')
            replacement = ('var object = T09Instance.makeWith(T09Instance.next());' if variant == 'efecto' else
                'var object = T09Instance.makeWith(1);' if variant == 'inicializador' else
                original + ' object = T09Instance.makeWith(1);')
            content = content.replace(original, replacement).replace(' != 12)', ' != 22)')
            if variant == 'efecto':
                content = content.replace(' return 0; } } }', ' if (T09Instance.callCount() != 1) return 58; return 0; } } }')
            path.write_text(content, encoding='utf-8')
            run('O' + str(opt) + '-argumento-' + variant + '-producer', [args.nsc, '--emit-project-oop-lib', 'Producer.nsproj',
                '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O' + str(opt), '-o', 'Producer-arg.lib'], producer)
            for visibility in ('con-encabezado', 'sin-encabezado'):
                consumer = conflict / visibility
                if visibility == 'sin-encabezado': (producer / 'api.h').rename(producer / 'api.h.oculto2')
                project = consumer / 'Consumer.nsproj'; content = project.read_text(encoding='utf-8')
                old = 'Include="../producer/Producer.lib"'
                if content.count(old) != 1: raise SystemExit('referencia de productor ausente')
                project.write_text(content.replace(old, 'Include="../producer/Producer-arg.lib"'), encoding='utf-8')
                command = [args.nsc, '--emit-project-oop-exe', 'Consumer.nsproj', '--cpp-clang-root=' + args.clang_root,
                    '--no-artifact-cache', '-O' + str(opt), '-o', 'arg.exe']
                result = subprocess.run(command, cwd=consumer, env=env, stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT, text=True, errors='replace')
                (conflict / ('resultado-' + visibility + '.log')).write_text('COMMAND: ' + repr(command) + '\nEXIT: '
                    + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
                if result.returncode != 0:
                    raise SystemExit('fábrica con argumento no aceptada: ' + str(conflict))
                run('O' + str(opt) + '-argumento-' + variant + '-' + visibility + '-run',
                    [str(consumer / 'arg.exe')], consumer)
                print('fábrica con argumento ' + variant + ' ' + visibility + ': ejecución correcta', flush=True)
    if args.precompilado:
        for variant in ('sobrecarga-ambigua', 'argumento-invalido', 'empate', 'aridad', 'variadica', 'cadena', 'cadena-sobrecarga', 'cadena-empate'):
            conflict = root / ('O' + str(opt) + '-' + variant); shutil.copytree(case, conflict)
            producer = conflict / 'producer'; shutil.copyfile(producer / 'api.h.oculto', producer / 'api.h')
            path = producer / 'Producer.ns'; content = path.read_text(encoding='utf-8')
            original = 'var object = T09Instance.make();'
            if content.count(original) != 1: raise SystemExit('inicializador ausente')
            if variant == 'sobrecarga-ambigua':
                content = ('namespace T09 { public class Bridge { public static int Run<T>(T value, int expected) { '
                    'int input = 3; var object = T09Instance.makeOver(value); '
                    'if (object->size0(&input, true) != expected) return 81; return 0; } } }\n')
            elif variant == 'empate':
                content = ('namespace T09 { public class Bridge { public static int Run<T>(T value) { '
                    'int input = 3; var object = T09Instance.makeTie(value, value); '
                    'if (object->size0(&input, true) != 22) return 81; return 0; } } }\n')
            elif variant == 'aridad':
                content = ('namespace T09 { public class Bridge { public static int Run<T>(T value) { '
                    'int input = 3; var object = T09Instance.makeArity(value); '
                    'if (object->size0(&input, true) != 22) return 81; return 0; } '
                    'public static int RunTwo<T>(T value) { int input = 3; '
                    'var object = T09Instance.makeArity(value, value); '
                    'if (object->size0(&input, true) != 35) return 82; return 0; } } }\n')
            elif variant == 'variadica':
                content = ('namespace T09 { public class Bridge { public static int Run<T>(T value) { '
                    'int input = 3; var object = T09Instance.makeVar(value, 9); '
                    'if (object->size0(&input, true) != 22) return 81; return 0; } } }\n')
            elif variant == 'cadena':
                content = ('namespace T09 { public class Bridge { public static int Run<T>(T value) { '
                    'int input = 3; if (T09Instance.makeWith(value)->size0(&input, true) != 22) return 81; '
                    'if (T09Instance.makeWith(T09Instance.next())->size0(&input, true) != 22) return 82; '
                    'if (T09Instance.callCount() != 1) return 83; '
                    'return 0; } } }\n')
            elif variant == 'cadena-sobrecarga':
                content = ('namespace T09 { public class Bridge { public static int Run<T>(T value, int expected) { '
                    'int input = 3; if (T09Instance.makeOver(value)->size0(&input, true) != expected) return 81; '
                    'return 0; } } }\n')
            elif variant == 'cadena-empate':
                content = ('namespace T09 { public class Bridge { public static int Run<T>(T value) { '
                    'int input = 3; if (T09Instance.makeTie(value, value)->size0(&input, true) != 22) return 81; '
                    'return 0; } } }\n')
            else:
                content = content.replace(original, 'var object = T09Instance.makeWith(null);')
            path.write_text(content, encoding='utf-8')
            run('O' + str(opt) + '-' + variant + '-producer', [args.nsc, '--emit-project-oop-lib', 'Producer.nsproj',
                '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O' + str(opt), '-o', 'Producer-over.lib'], producer)
            for visibility in ('con-encabezado', 'sin-encabezado'):
                consumer = conflict / visibility
                if visibility == 'sin-encabezado': (producer / 'api.h').rename(producer / 'api.h.oculto2')
                if variant in ('sobrecarga-ambigua', 'cadena-sobrecarga'):
                    (consumer / 'Main.ns').write_text('int Main() { if (T09.Bridge.Run<int>(1, 22) != 0) return 82; '
                        'if (T09.Bridge.Run<long>((long)1, 35) != 0) return 83; return 0; }\n', encoding='utf-8')
                elif variant in ('empate', 'variadica', 'cadena', 'cadena-empate'):
                    (consumer / 'Main.ns').write_text('int Main() { return T09.Bridge.Run<int>(1); }\n', encoding='utf-8')
                elif variant == 'aridad':
                    (consumer / 'Main.ns').write_text('int Main() { if (T09.Bridge.Run<int>(1) != 0) return 83; '
                        'return T09.Bridge.RunTwo<int>(1); }\n', encoding='utf-8')
                project = consumer / 'Consumer.nsproj'; content = project.read_text(encoding='utf-8')
                old = 'Include="../producer/Producer.lib"'
                if content.count(old) != 1: raise SystemExit('referencia de productor ausente')
                project.write_text(content.replace(old, 'Include="../producer/Producer-over.lib"'), encoding='utf-8')
                command = [args.nsc, '--emit-project-oop-exe', 'Consumer.nsproj', '--cpp-clang-root=' + args.clang_root,
                    '--no-artifact-cache', '-O' + str(opt), '-o', 'over.exe']
                result = subprocess.run(command, cwd=consumer, env=env, stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT, text=True, errors='replace')
                (conflict / ('resultado-' + visibility + '.log')).write_text('COMMAND: ' + repr(command) + '\nEXIT: '
                    + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
                if variant in ('sobrecarga-ambigua', 'aridad', 'variadica', 'cadena', 'cadena-sobrecarga'):
                    if result.returncode != 0:
                        raise SystemExit('sobrecargas no aceptadas: ' + str(conflict))
                    run('O' + str(opt) + '-sobrecarga-' + visibility + '-run', [str(consumer / 'over.exe')], consumer)
                    print('sobrecargas ' + visibility + ': ejecuciones correctas', flush=True)
                elif variant == 'argumento-invalido':
                    expected = "no viable overload of namespace function 'T09Instance.makeWith'"
                    if result.returncode == 0 or expected not in result.stdout:
                        raise SystemExit('argumento inválido no rechazado: ' + str(conflict))
                    print(variant + ' ' + visibility + ': rechazo correcto', flush=True)
                else:
                    if result.returncode == 0 or 'ambig' not in result.stdout.lower():
                        raise SystemExit('sobrecarga empatada no rechazada como ambigua: ' + str(conflict))
                    print('empate ' + visibility + ': rechazo correcto', flush=True)
    print(f'O{opt}: control C++ y NativeSharp pasaron', flush=True)
print('OK: métodos de instancia; evidencia:', root)
