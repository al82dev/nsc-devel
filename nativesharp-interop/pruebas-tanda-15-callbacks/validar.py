"""T15-008 a T15-012: funciones NativeSharp como callbacks de una biblioteca C++.

La biblioteca recibe punteros a funcion C++ (`using Cb = int(*)(int)`): uno lo llama durante la llamada,
otro lo guarda y lo llama una llamada posterior, y un tercero recibe ademas un `void*` de contexto con el
estado que NativeSharp le pasa (el patron de callback con estado sin capturas). NSC emite el adaptador de una
lambda sin capturas con el ABI de Clang. La biblioteca no necesita el runtime de C. Compara un control C++
compilado por Clang 23.1.0 con el consumidor NativeSharp, O0-O3, en estatica y DLL. Una lambda que captura
estado se rechaza con su motivo; la contraparte pasa el estado por el contexto.
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
root = Path(tempfile.mkdtemp(prefix='nsc-t15-callbacks-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)

HEADER = '''#pragma once
#if defined(T15_EXPORT)
#define API __declspec(dllexport)
#elif defined(T15_IMPORT)
#define API __declspec(dllimport)
#else
#define API
#endif
namespace T15 {
using Cb = int(*)(int);
using CtxCb = int(*)(void*, int);
API int apply(Cb f, int x);
API void store(Cb f);
API int callStored(int x);
API int applyCtx(CtxCb f, void* ctx, int x);
API int twice(Cb f, int x);
}
'''
SOURCE = '''#include "api.h"
namespace T15 {
static Cb saved = nullptr;
int apply(Cb f, int x) { return f(x); }
void store(Cb f) { saved = f; }
int callStored(int x) { return saved(x) + saved(x + 1); }
int applyCtx(CtxCb f, void* ctx, int x) { return f(ctx, x); }
int twice(Cb f, int x) { return f(f(x)); }
}
'''
CONTROL = '''#include "api.h"
struct P { int k; };
int main() {
    int r = T15::apply([](int v) { return v * 2; }, 21);
    T15::store([](int v) { return v + 1; });
    r += T15::callStored(10);
    P p{40};
    r += T15::applyCtx([](void* c, int v) { return static_cast<P*>(c)->k + v; }, &p, 2);
    r += T15::twice([](int v) { return v * 3; }, 2);
    return r;
}
'''
PRELUDE = 'struct P { public int k; }\n'
NS_BODY = ('T15.Cb dbl = (int v) => v * 2; int r = T15.apply(dbl, 21); '
           'T15.Cb inc = (int v) => v + 1; T15.store(inc); r += T15.callStored(10); '
           'P p; p.k = 40; T15.CtxCb ctx = (void* c, int v) => ((P*)c)->k + v; r += T15.applyCtx(ctx, &p, 2); '
           'T15.Cb tri = (int v) => v * 3; r += T15.twice(tri, 2); return r;')
EXPECTED = 42 + 11 + 12 + 42 + 18
DIRECT = 5


def run(label, command, cwd, expected=0):
    result = subprocess.run(command, cwd=cwd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, errors='replace')
    log = root / (label + '.log')
    log.write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' + result.stdout,
                   encoding='utf-8')
    if expected is not None and ((expected == 0 and result.returncode) or (expected != 0 and result.returncode == 0)):
        print(result.stdout[-3000:], flush=True)
        raise SystemExit(label + ': codigo ' + str(result.returncode) + ' inesperado; evidencia: ' + str(log))
    return result


def declarations(ir):
    """Physical parameter and return types of every T15 declaration, keyed by mangled name."""
    found = {}
    for line in ir.splitlines():
        match = re.match(r'(?:declare|define)\s+(.*?)\s+@"?(\?[^"(]*T15@@[^"(]*)"?\((.*)\)', line)
        if not match:
            continue
        head, symbol, parameters = match.groups()
        result = [w for w in head.split() if w not in ('dso_local', 'dllimport', 'noundef', 'nonnull', 'zeroext',
                                                       'signext') and not w.startswith(('dereferenceable', 'align'))]
        found[symbol] = (result[-1] if result else '', tuple(p.split()[0] for p in parameters.split(', ') if p.strip()))
    return found


def project(folder, body):
    runtime = ' Runtime="api.dll"' if args.dll else ''
    (folder / 'Main.ns').write_text(PRELUDE + 'int Main() { ' + body + ' }\n', encoding='utf-8')
    (folder / 'App.nsproj').write_text(
        '<Project><PropertyGroup><Name>T15App</Name></PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
        '<CppLibraryReference Header="api.h" Language="C++" Standard="c++20" Library="api.lib"' + runtime + ' />'
        '</ItemGroup></Project>', encoding='utf-8')


if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version'], root).stdout:
    raise SystemExit('Se requiere Clang 23.1.0')
for opt in range(4):
    case = root / f'O{opt}'
    case.mkdir()
    (case / 'api.h').write_text(HEADER, encoding='utf-8')
    (case / 'api.cpp').write_text(SOURCE, encoding='utf-8')
    (case / 'Control.cpp').write_text(CONTROL, encoding='utf-8')
    flags = ['-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-O' + str(opt)]
    run(f'O{opt}-api', ['clang++.exe', *flags, *(['-DT15_EXPORT'] if args.dll else []), '-c', 'api.cpp', '-o', 'api.obj'], case)
    if args.dll:
        run(f'O{opt}-dll', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib', '/out:api.dll',
                            '/implib:api.lib', 'api.obj'], case)
        (case / 'api.h').write_text('#define T15_IMPORT 1\n' + HEADER, encoding='utf-8')
    else:
        run(f'O{opt}-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], case)
    control_ir = run(f'O{opt}-control-ir', ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-O0', '-S',
                                            '-emit-llvm', 'Control.cpp', '-o', '-'], case).stdout
    run(f'O{opt}-control-obj', ['clang++.exe', *flags, '-c', 'Control.cpp', '-o', 'control.obj'], case)
    run(f'O{opt}-control-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console', '/nodefaultlib',
                                 '/out:control.exe', 'control.obj', 'api.lib'], case)
    control = run(f'O{opt}-control-run', [str(case / 'control.exe')], case, expected=None).returncode
    if control != EXPECTED:
        raise SystemExit(f'O{opt}: el control C++ devolvio {control}, se esperaba {EXPECTED}; evidencia: {root}')
    project(case, NS_BODY)
    ir = run(f'O{opt}-ir', [args.nsc, '--dump-project-oop-llvm', 'App.nsproj', '--cpp-clang-root=' + args.clang_root], case).stdout
    clang, nsc = declarations(control_ir), declarations(ir)
    if len(nsc) != DIRECT or any(clang.get(symbol) != shape for symbol, shape in nsc.items()):
        raise SystemExit(f'O{opt}: declaraciones distintas de Clang: {nsc} frente a {clang}; evidencia: {root}')
    run(f'O{opt}-nsc', [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
                        '--no-artifact-cache', '-O' + str(opt), '-o', 'app.exe'], case)
    result = run(f'O{opt}-run', [str(case / 'app.exe')], case, expected=None).returncode
    if result != EXPECTED:
        raise SystemExit(f'O{opt}: NativeSharp devolvio {result}, Clang {EXPECTED}; evidencia: {root}')
    print(f'O{opt}: control C++ y NativeSharp coinciden en {result} ({DIRECT} declaraciones iguales a las de Clang)',
          flush=True)
case = root / 'O0'
project(case, 'int k = 40; T15.Cb f = (int v) => v + k; return T15.apply(f, 2);')
output = run('negativo-captura', [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
                                  '--no-artifact-cache', '-o', 'neg.exe'], case, expected=1).stdout
if 'must not capture state' not in output:
    print(output[-2000:], flush=True)
    raise SystemExit(f'negativo captura: falta el diagnostico esperado; evidencia: {root}')
print('negativo captura: una lambda con estado se rechaza con su motivo; la contraparte usa el contexto', flush=True)
print('OK: callbacks NativeSharp llamados por C++, frente a Clang; evidencia:', root)
