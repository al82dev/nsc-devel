"""T18-011: constructores C++ que pueden lanzar en locales NativeSharp.

La biblioteca define `Obj`, cuyo constructor (sin `noexcept`) lanza `7` con un argumento negativo y anota `c`;
su destructor anota `d`. Un callback NativeSharp construye `a(1)` y luego `b(-1)`: la excepcion atraviesa el
frame NativeSharp hasta el `catch` de la biblioteca, `a` se destruye y `b`, que nunca se construyo, no. La
anotacion debe ser `cd` y el resultado 107, igual que con el mismo callback escrito en C++ (Clang 23.1.0),
O0-O3. Sin excepcion, los dos locales se construyen y destruyen en orden inverso. Un local cuyo destructor
es `noexcept(false)` sigue rechazado con su motivo. En estatica el
ejecutable arranca por el runtime de C (`--crt-startup`); con --dll la biblioteca es una DLL con su propio
runtime.
"""
import argparse
import os
import shutil
import subprocess
import tempfile
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
parser.add_argument('--dll', action='store_true')
args = parser.parse_args()
root = Path(tempfile.mkdtemp(prefix='nsc-t18-constructores-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)
TOOLSET = Path('A:/vc2026-runtime/toolsets/14.52.36815')
SDK = Path('A:/vc2026-runtime/windows-sdk/10.0.28000.0')
CRT = [TOOLSET / 'lib/x64/libcmt.lib', TOOLSET / 'lib/x64/libvcruntime.lib', TOOLSET / 'lib/x64/libcpmt.lib',
       SDK / 'lib/ucrt/x64/libucrt.lib', SDK / 'lib/um/x64/kernel32.lib']
CXX = ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20']
HEADER = '''#pragma once
#if defined(T18_EXPORT)
#define API __declspec(dllexport)
#elif defined(T18_IMPORT)
#define API __declspec(dllimport)
#else
#define API
#endif
namespace T18 {
using Cb = int(*)();
class API Obj {
public:
    explicit Obj(int v);
    ~Obj();
    int value() const noexcept { return v_; }
private:
    int v_;
};
API int run(Cb f);
API int trace() noexcept;
class API Risky {
public:
    Risky();
    ~Risky() noexcept(false);
};
}
'''
SOURCE = '''#include "api.h"
namespace { int marks = 0; int count = 0; }
namespace T18 {
Obj::Obj(int v) : v_(v) { if (v < 0) throw 7; marks = marks * 10 + 1; ++count; }
Obj::~Obj() { marks = marks * 10 + 2; }
int run(Cb f) { try { return f(); } catch (int e) { return 100 + e; } }
int trace() noexcept { return marks; }
Risky::Risky() {}
Risky::~Risky() noexcept(false) { if (count < 0) throw 9; }
}
'''
CONTROL = '''#include "api.h"
static int throwing() { T18::Obj a(1); T18::Obj b(-1); return a.value() + b.value(); }
static int plain() { T18::Obj a(1); T18::Obj b(2); return a.value() + b.value(); }
int main() { int r = T18::run(throwing); int t = T18::trace(); int s = T18::run(plain); return r * 1000000 + t * 1 + 0 * s + (s == 3 ? 0 : 1); }
'''
CONTROL_FIELDS = '''#include "api.h"
struct Holder { T18::Obj a{1}; T18::Obj b{-1}; };
static int throwing() { Holder h; return h.a.value(); }
int main() { int r = T18::run(throwing); int t = T18::trace(); return r * 1000000 + t; }
'''
# X2-06: the same with fields of a NativeSharp class: `a` is built, `b` throws, `a` is destroyed and the object's
# storage released. / Lo mismo con campos de una clase NativeSharp.
NS_FIELDS = '''class Holder { public T18.Obj a = new T18.Obj(1); public T18.Obj b = new T18.Obj(-1); }
int Throwing() { var h = new Holder(); return h.a.value(); }
int Main()
{
    T18.Cb throwing = () => Throwing();
    int r = T18.run(throwing);
    return r * 1000000 + T18.trace();
}
'''
NS = '''int Throwing() { T18.Obj a = new T18.Obj(1); T18.Obj b = new T18.Obj(-1); return a.value() + b.value(); }
int Plain() { T18.Obj a = new T18.Obj(1); T18.Obj b = new T18.Obj(2); return a.value() + b.value(); }
int Main()
{
    T18.Cb throwing = () => Throwing();
    T18.Cb plain = () => Plain();
    int r = T18.run(throwing);
    int t = T18.trace();
    int s = T18.run(plain);
    return r * 1000000 + t + (s == 3 ? 0 : 1);
}
'''


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
# a(1) builds (1), b(-1) throws, a is destroyed (2): 12. / a(1) se construye, b(-1) lanza, a se destruye: 12.
EXPECTED = 107 * 1000000 + 12
for opt in range(4):
    case = root / f'O{opt}'
    case.mkdir()
    (case / 'api.h').write_text(HEADER, encoding='utf-8')
    (case / 'api.cpp').write_text(SOURCE, encoding='utf-8')
    (case / 'control.cpp').write_text(CONTROL, encoding='utf-8')
    run(f'O{opt}-api', [*CXX, '-O' + str(opt), *(['-DT18_EXPORT'] if args.dll else []), '-c', 'api.cpp', '-o', 'api.obj'], case)
    if args.dll:
        run(f'O{opt}-dll', ['lld-link.exe', '/nologo', '/dll', '/out:api.dll', '/implib:api.lib', 'api.obj',
                            *[str(p) for p in CRT]], case)
        (case / 'api.h').write_text('#define T18_IMPORT 1\n' + HEADER, encoding='utf-8')
    else:
        run(f'O{opt}-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], case)
    run(f'O{opt}-control-obj', [*CXX, '-O' + str(opt), '-c', 'control.cpp', '-o', 'control.obj'], case)
    run(f'O{opt}-control-link', ['lld-link.exe', '/nologo', '/subsystem:console', '/out:control.exe', 'control.obj', 'api.lib',
                                 *[str(p) for p in CRT]], case)
    control = run(f'O{opt}-control-run', [str(case / 'control.exe')], case, expected=None).returncode
    if control != EXPECTED:
        raise SystemExit(f'O{opt}: el control C++ devolvio {control}, se esperaba {EXPECTED}; evidencia: {root}')
    (case / 'Main.ns').write_text(NS, encoding='utf-8')
    runtime = ' Runtime="api.dll"' if args.dll else ''
    (case / 'App.nsproj').write_text(
        '<Project><PropertyGroup><Name>T18Ctor</Name></PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
        '<CppLibraryReference Header="api.h" Language="C++" Standard="c++20" Library="api.lib"' + runtime + ' />'
        '</ItemGroup></Project>', encoding='utf-8')
    command = [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
               '--no-artifact-cache', '-O' + str(opt)]
    extra = {'NSC_LINKER': 'nslink.exe'}
    if not args.dll:
        command.append('--crt-startup')
        for path in CRT:
            command += ['--link-lib', str(path)]
        extra = {'NSC_LINKER': 'lld-link.exe'}
    run(f'O{opt}-nsc', command + ['-o', 'app.exe'], case, extra_env=extra)
    result = run(f'O{opt}-run', [str(case / 'app.exe')], case, expected=None).returncode
    if result != control:
        raise SystemExit(f'O{opt}: NativeSharp devolvio {result}, Clang {control}; evidencia: {root}')
    print(f'O{opt}: NativeSharp y Clang devuelven {result}: la excepcion del constructor llega al catch, '
          f'a se destruye y b no', flush=True)
    (case / 'control-fields.cpp').write_text(CONTROL_FIELDS, encoding='utf-8')
    run(f'O{opt}-fields-control-obj', [*CXX, '-O' + str(opt), '-c', 'control-fields.cpp', '-o', 'control-fields.obj'], case)
    run(f'O{opt}-fields-control-link', ['lld-link.exe', '/nologo', '/subsystem:console', '/out:control-fields.exe',
                                        'control-fields.obj', 'api.lib', *[str(p) for p in CRT]], case)
    fieldsControl = run(f'O{opt}-fields-control-run', [str(case / 'control-fields.exe')], case, expected=None).returncode
    if fieldsControl != EXPECTED:
        raise SystemExit(f'O{opt}: el control C++ de campos devolvio {fieldsControl}; evidencia: {root}')
    (case / 'Main.ns').write_text(NS_FIELDS, encoding='utf-8')
    run(f'O{opt}-fields-nsc', command + ['-o', 'fields.exe'], case, extra_env=extra)
    fields = run(f'O{opt}-fields-run', [str(case / 'fields.exe')], case, expected=None).returncode
    if fields != fieldsControl:
        raise SystemExit(f'O{opt}: campos de clase: NativeSharp devolvio {fields}, Clang {fieldsControl}; evidencia: {root}')
    print(f'O{opt}: campos C++ de una clase NativeSharp: el constructor que lanza deja a destruido y b sin construir, '
          f'como Clang ({fields})', flush=True)
# A destructor that may throw stays rejected. / Un destructor que puede lanzar sigue rechazado.
case = root / 'O0'
(case / 'Main.ns').write_text('int Main() { T18.Risky r = new T18.Risky(); return 0; }\n', encoding='utf-8')
output = run('negativo-destructor', [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
                                     '--no-artifact-cache', '-o', 'riesgo.exe'], case, expected=1,
             extra_env={'NSC_LINKER': 'nslink.exe' if args.dll else 'lld-link.exe'}).stdout
if 'requires a non-throwing destructor' not in output:
    print(output[-2000:], flush=True)
    raise SystemExit(f'negativo: un destructor noexcept(false) no se rechaza con su motivo; evidencia: {root}')
print('negativo: un local con destructor noexcept(false) se rechaza con su motivo', flush=True)
print('OK: constructores C++ que lanzan en locales NativeSharp, frente a Clang; evidencia:', root)
