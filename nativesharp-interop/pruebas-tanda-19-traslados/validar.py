"""T19-036: fixtures de capacidades trasladadas entre tandas sin evidencia propia.

Cada caso compara un programa NativeSharp con un control C++ de Clang 23.1.0, O0-O3, con la biblioteca como
estatica y, con --dll, como DLL. Los casos que todavia no funcionan estan en `casos_pendientes.py` y pasan a
este archivo cuando NativeSharp los consume como Clang (registro t19-traslados-verificacion.md):

- `tls-campo`: campo estatico `constinit thread_local` leido y escrito desde NativeSharp (estatica, con el CRT). Con
  --dll el caso comprueba que Clang tambien rechaza un `thread_local` `dllimport`: el ABI de Microsoft no lo
  permite.

Uso: validar.py --nsc <nsc.exe> --clang-root <raiz de Clang> [--dll] [--caso <nombre>]
"""
import argparse
import os
import subprocess
import tempfile
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
parser.add_argument('--dll', action='store_true')
parser.add_argument('--caso')
args = parser.parse_args()
root = Path(tempfile.mkdtemp(prefix='nsc-t19-traslados-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)
TOOLSET = Path('A:/vc2026-runtime/toolsets/14.52.36815')
SDK = Path('A:/vc2026-runtime/windows-sdk/10.0.28000.0')
CRT = [TOOLSET / 'lib/x64/libcmt.lib', TOOLSET / 'lib/x64/libvcruntime.lib', TOOLSET / 'lib/x64/libcpmt.lib',
       SDK / 'lib/ucrt/x64/libucrt.lib', SDK / 'lib/um/x64/kernel32.lib']
CXX = ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20']
API = '''#if defined(T19_EXPORT)
#define API __declspec(dllexport)
#elif defined(T19_IMPORT)
#define API __declspec(dllimport)
#else
#define API
#endif
'''
CASES = {
    'tls-campo': {
        'header': '''namespace T {
struct S { static constinit thread_local int counter; };
API int bump() noexcept;
}
''',
        'source': '''namespace T {
constinit thread_local int S::counter = 5;
int bump() noexcept { S::counter = S::counter + 1; return S::counter; }
}
''',
        'cpp': 'int a = T::S::counter; T::S::counter = 40; int b = T::bump(); return a * 100 + b;',
        'ns': 'int a = T.S.counter; T.S.counter = 40; int b = T.bump(); return a * 100 + b;',
        'crt': True,
    },
}


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


def project(folder, body, dll):
    runtime = ' Runtime="api.dll"' if dll else ''
    (folder / 'Main.ns').write_text('int Main()\n{\n    ' + body + '\n}\n', encoding='utf-8')
    (folder / 'App.nsproj').write_text(
        '<Project><PropertyGroup><Name>T19Traslados</Name></PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
        '<CppLibraryReference Header="api.h" Language="C++" Standard="c++20" Library="api.lib"' + runtime + ' />'
        '</ItemGroup></Project>', encoding='utf-8')


def tls_dll_rejected(name, case):
    # The Microsoft ABI does not allow dllimport thread-local data: Clang rejects the header too.
    # El ABI de Microsoft no permite datos thread_local dllimport: Clang tambien rechaza el encabezado.
    folder = root / (name + '-dll')
    folder.mkdir()
    exported = case['header'].replace('static constinit thread_local', 'API static constinit thread_local')
    header = '#define T19_IMPORT 1\n' + API + exported
    (folder / 'api.h').write_text(header, encoding='utf-8')
    (folder / 'control.cpp').write_text('#include "api.h"\nint main() { return T::S::counter; }\n', encoding='utf-8')
    clang = run(name + '-dll-control', [*CXX, '-c', 'control.cpp', '-o', 'control.obj'], folder, expected=1).stdout
    if 'thread' not in clang or 'dllimport' not in clang:
        print(clang[-2000:], flush=True)
        raise SystemExit(name + ': Clang no rechaza el thread_local dllimport; evidencia: ' + str(root))
    print(name + ': Clang rechaza un thread_local dllimport (ABI de Microsoft); no aplica en DLL', flush=True)


def check(name, case):
    if args.dll and case['crt']:
        tls_dll_rejected(name, case)
        return
    library = root / (name + '-lib')
    library.mkdir()
    (library / 'api.h').write_text(API + case['header'], encoding='utf-8')
    (library / 'api.cpp').write_text('#include "api.h"\n' + case['source'], encoding='utf-8')
    run(name + '-api', [*CXX, '-O2', *(['-DT19_EXPORT'] if args.dll else []), '-c', 'api.cpp', '-o', 'api.obj'],
        library)
    if args.dll:
        run(name + '-api-dll', ['lld-link.exe', '/nologo', '/dll', '/noentry', '/nodefaultlib', '/out:api.dll',
                                '/implib:api.lib', 'api.obj'], library)
        header = '#define T19_IMPORT 1\n' + API + case['header']
    else:
        run(name + '-api-lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], library)
        header = API + case['header']
    for opt in range(4):
        folder = root / f'{name}-O{opt}'
        folder.mkdir()
        (folder / 'api.h').write_text(header, encoding='utf-8')
        for item in ('api.lib', *(('api.dll',) if args.dll else ())):
            (folder / item).write_bytes((library / item).read_bytes())
        # Without the CRT the control defines the floating-point marker itself; with it, libcmt.lib does.
        # Sin el CRT el control define el marcador de punto flotante; con el CRT lo define libcmt.lib.
        marker = '' if case['crt'] else 'extern "C" __declspec(selectany) int _fltused = 0;\n'
        (folder / 'control.cpp').write_text('#include "api.h"\n' + marker + 'int main() { ' + case['cpp'] + ' }\n',
                                            encoding='utf-8')
        if case['crt']:
            run(f'{name}-O{opt}-control', [*CXX, '-O' + str(opt), '-c', 'control.cpp', '-o', 'control.obj'], folder)
            run(f'{name}-O{opt}-control-link', ['lld-link.exe', '/nologo', '/subsystem:console', '/out:control.exe',
                                                'control.obj', 'api.lib', *[str(p) for p in CRT]], folder)
        else:
            run(f'{name}-O{opt}-control', [*CXX, '-O' + str(opt), 'control.cpp', 'api.lib', '-fuse-ld=lld',
                                           '-nostdlib', '-Wl,/entry:main', '-o', 'control.exe'], folder)
        control = run(f'{name}-O{opt}-control-run', [str(folder / 'control.exe')], folder, expected=None).returncode
        project(folder, case['ns'], args.dll)
        command = [args.nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + args.clang_root,
                   '--no-artifact-cache', '-O' + str(opt)]
        extra = {}
        if case['crt']:
            command.append('--crt-startup')
            for item in CRT:
                command += ['--link-lib', str(item)]
            extra = {'NSC_LINKER': 'lld-link.exe'}
        run(f'{name}-O{opt}-nsc', command + ['-o', 'app.exe'], folder, extra_env=extra)
        result = run(f'{name}-O{opt}-run', [str(folder / 'app.exe')], folder, expected=None).returncode
        if result != control:
            raise SystemExit(f'{name} O{opt}: NativeSharp devolvio {result}, Clang {control}; evidencia: {root}')
        print(f'{name} O{opt}: NativeSharp y Clang devuelven {result}', flush=True)


if 'clang version 23.1.0' not in run('clang-version', ['clang.exe', '--version'], root).stdout:
    raise SystemExit('Se requiere Clang 23.1.0')
for name, case in CASES.items():
    if args.caso and args.caso != name:
        continue
    check(name, case)
print('OK: traslados de T19-036 frente a Clang; evidencia:', root)
