"""T13-019 y T13-020: COMDAT y la misma especializacion pedida desde dos modulos NativeSharp.

Una biblioteca C++ (Clang 23.1.0, O0) usa Box<int>::get y Box<int>::twice en una funcion propia, asi que su
objeto define esas funciones en secciones COMDAT. Un modulo NativeSharp precompilado (Producer.lib) y el
ejecutable que lo consume escriben T13.Box<int> y llaman a los mismos miembros, que Clang emite en cada
modulo. Se comprueba con llvm-readobj que la biblioteca C++ y el producto NativeSharp definen esas funciones
en secciones COMDAT de seleccion Any, y que el ejecutable enlaza una sola definicion y da el resultado del
control C++ con NSLink y con lld-link, de O0 a O3, en estatica y DLL.
"""
import argparse
import os
import re
import shutil
import subprocess
import tempfile
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
parser.add_argument('--dll', action='store_true')
args = parser.parse_args()
root = Path(tempfile.mkdtemp(prefix='nsc-t13-comdat-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)

HEADER = '''#pragma once
#if defined(T13_EXPORT)
#define API __declspec(dllexport)
#elif defined(T13_IMPORT)
#define API __declspec(dllimport)
#else
#define API
#endif
namespace T13 {
template <class T> struct Box {
    T v;
    T get() const noexcept { return v; }
    T twice() const noexcept { return v + v; }
};
API int libGet(const Box<int>* b) noexcept;
}
'''
SOURCE = '''#include "api.h"
namespace T13 {
int libGet(const Box<int>* b) noexcept { return b->get() + b->twice(); }
}
'''
CONTROL = '''#include "api.h"
static int producerUse(int x) { T13::Box<int> b; b.v = x; return b.get() + b.twice() + T13::libGet(&b); }
int main() { T13::Box<int> b; b.v = 2; return producerUse(5) + b.get() + b.twice(); }
'''
PRODUCER = '''namespace P13 {
public class Lib {
    public static int Use(int x) { T13.Box<int> b; b.v = x; return b.get() + b.twice() + T13.libGet(&b); }
}
}
'''
CONSUMER = 'int Main() { T13.Box<int> b; b.v = 2; return P13.Lib.Use(5) + b.get() + b.twice(); }\n'
EXPECTED = 36
MEMBERS = ['?get@?$Box@H@T13@@QEBAHXZ', '?twice@?$Box@H@T13@@QEBAHXZ']


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


def comdat_any(label, path, cwd):
    """Each member is defined, in every object of the file that defines it, in a COMDAT section whose
    selection is Any: the section symbol of that section carries the selection."""
    text = run(label, ['llvm-readobj.exe', '--symbols', str(path)], cwd).stdout
    found = 0
    for obj in re.split(r'\nFile: ', text):
        blocks = obj.split('Symbol {')
        selection = {}
        for block in blocks:
            section = re.search(r'Section: \S+ \((\d+)\)', block)
            chosen = re.search(r'Selection: (\w+)', block)
            if section and chosen:
                selection[section.group(1)] = chosen.group(1)
        for member in MEMBERS:
            for block in blocks:
                if ('Name: ' + member + '\n') not in block:
                    continue
                section = re.search(r'Section: \S+ \((\d+)\)', block)
                if not section or section.group(1) == '0':
                    continue  # an undefined reference / una referencia sin definir
                if selection.get(section.group(1)) != 'Any':
                    raise SystemExit(f'{label}: {member} no esta en una seccion COMDAT Any en {path}; '
                                     f'evidencia: {root}')
                found += 1
    if found < len(MEMBERS):
        raise SystemExit(f'{label}: {path} no define {MEMBERS}; evidencia: {root}')
    return found


def project(name, references, source, library=False):
    output = '<OutputType>Library</OutputType>' if library else ''
    return ('<Project><PropertyGroup><Name>' + name + '</Name>' + output + '</PropertyGroup><ItemGroup>'
            '<Compile Include="' + source + '" />' + references + '</ItemGroup></Project>')


def cpp_reference(prefix):
    runtime = ' Runtime="' + prefix + 'api.dll"' if args.dll else ''
    return ('<CppLibraryReference Header="' + prefix + 'api.h" Language="C++" Standard="c++20" Library="'
            + prefix + 'api.lib"' + runtime + ' />')


if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version'], root).stdout:
    raise SystemExit('Se requiere Clang 23.1.0')
for opt in range(4):
    case = root / f'O{opt}'
    case.mkdir()
    (case / 'api.h').write_text(HEADER, encoding='utf-8')
    (case / 'api.cpp').write_text(SOURCE, encoding='utf-8')
    (case / 'Control.cpp').write_text(CONTROL, encoding='utf-8')
    # The library is built at O0, so its object keeps the members it uses.
    run(f'O{opt}-api', ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-O0',
                        *(['-DT13_EXPORT'] if args.dll else []), '-c', 'api.cpp', '-o', 'api.obj'], case)
    members = comdat_any(f'O{opt}-api-comdat', case / 'api.obj', case)
    if args.dll:
        run(f'O{opt}-dll', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib', '/out:api.dll',
                            '/implib:api.lib', 'api.obj'], case)
        (case / 'api.h').write_text('#define T13_IMPORT 1\n' + HEADER, encoding='utf-8')
    else:
        run(f'O{opt}-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], case)
    run(f'O{opt}-control-obj', ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-O' + str(opt),
                                '-c', 'Control.cpp', '-o', 'control.obj'], case)
    run(f'O{opt}-control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console', '/nodefaultlib',
                                 '/out:control.exe', 'control.obj', 'api.lib'], case)
    control = run(f'O{opt}-control-run', [str(case / 'control.exe')], case, expected=None).returncode
    if control != EXPECTED:
        raise SystemExit(f'O{opt}: el control C++ devolvio {control}, se esperaba {EXPECTED}; evidencia: {root}')
    producer = case / 'producer'
    producer.mkdir()
    (producer / 'Producer.ns').write_text(PRODUCER, encoding='utf-8')
    (producer / 'Producer.nsproj').write_text(project('P13', cpp_reference('../'), 'Producer.ns', True),
                                              encoding='utf-8')
    run(f'O{opt}-producer', [args.nsc, '--emit-project-oop-lib', 'Producer.nsproj',
                             '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O0',
                             '-o', 'Producer.lib'], producer)
    members += comdat_any(f'O{opt}-producer-comdat', producer / 'Producer.lib', producer)
    for linker in ('nslink.exe', 'lld-link.exe'):
        consumer = case / ('consumer-' + linker.split('.')[0])
        consumer.mkdir()
        (consumer / 'Main.ns').write_text(CONSUMER, encoding='utf-8')
        references = '<LibraryReference Include="../producer/Producer.lib" />' + cpp_reference('../')
        (consumer / 'App.nsproj').write_text(project('T13App', references, 'Main.ns'), encoding='utf-8')
        if args.dll:
            shutil.copyfile(case / 'api.dll', consumer / 'api.dll')
        tag = f'O{opt}-{linker.split(".")[0]}'
        selected = run(tag + '-linker', [args.nsc, '--print-linker'], consumer,
                       extra_env={'NSC_LINKER': linker}).stdout.splitlines()[0].lower()
        if not selected.startswith('linker:') or not selected.endswith(linker):
            raise SystemExit(f'{tag}: NSC no selecciona {linker} ({selected}); evidencia: {root}')
        run(tag + '-build', [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
                             '--no-artifact-cache', '-O' + str(opt), '-o', 'app.exe'], consumer,
            extra_env={'NSC_LINKER': linker})
        result = run(tag + '-run', [str(consumer / 'app.exe')], consumer, expected=None).returncode
        if result != EXPECTED:
            raise SystemExit(f'{tag}: NativeSharp devolvio {result}, Clang {EXPECTED}; evidencia: {root}')
        print(f'{tag}: control C++ y NativeSharp coinciden; {members} definiciones COMDAT Any comprobadas',
              flush=True)
print('OK: COMDAT y la misma especializacion desde dos modulos, frente a Clang; evidencia:', root)
