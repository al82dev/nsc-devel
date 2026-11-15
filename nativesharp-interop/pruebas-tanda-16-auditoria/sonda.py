"""Sonda de auditoria T16 (no es una prueba de soporte): enlace, arranque y ejecucion de bibliotecas C++.

Cada caso compila una biblioteca con Clang 23.1.0 y un consumidor NativeSharp; imprime el codigo de salida o
el primer diagnostico, y para los casos con observacion externa, el contenido del archivo que escribe la
biblioteca con Win32 (sin el CRT).
"""
import os, subprocess, sys, tempfile, shutil
from pathlib import Path
nsc, clang_root = sys.argv[1], sys.argv[2]
only = set(sys.argv[3:])
root = Path(tempfile.mkdtemp(prefix='nsc-t16-sonda-', dir='A:/temp'))
env = os.environ.copy(); env['TEMP'] = env['TMP'] = str(root)
env['NSC_WINDOWS_RUNTIME_LIBRARY'] = 'A:/nsc-devel/build-tanda09/generated/NativeSharp.Runtime.Win32.lib'
print('Evidencia:', root, flush=True)
TS = Path('A:/vc2026-runtime/toolsets/14.52.36815')
SDK = Path('A:/vc2026-runtime/windows-sdk/10.0.28000.0')
CRT = [TS / 'lib/x64/libcmt.lib', TS / 'lib/x64/libvcruntime.lib', TS / 'lib/x64/libcpmt.lib',
       SDK / 'lib/ucrt/x64/libucrt.lib', SDK / 'lib/um/x64/kernel32.lib', SDK / 'lib/um/x64/synchronization.lib']
CXX = ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-isystem', str(TS / 'include'),
       '-isystem', str(SDK / 'include/ucrt'), '-isystem', str(SDK / 'include/um'), '-isystem', str(SDK / 'include/shared')]
MARK = '''extern "C" __declspec(dllimport) void* __stdcall CreateFileA(const char*, unsigned long, unsigned long, void*, unsigned long, unsigned long, void*);
extern "C" __declspec(dllimport) int __stdcall WriteFile(void*, const void*, unsigned long, unsigned long*, void*);
extern "C" __declspec(dllimport) int __stdcall CloseHandle(void*);
static void mark(const char* text, unsigned long n) { void* h = CreateFileA("marca.txt", 4, 0, nullptr, 4, 0x80, nullptr);
  unsigned long w = 0; if (h != (void*)-1) { SetFilePointerEx_(h); WriteFile(h, text, n, &w, nullptr); CloseHandle(h); } }
'''
MARK = '''extern "C" __declspec(dllimport) void* __stdcall CreateFileA(const char*, unsigned long, unsigned long, void*, unsigned long, unsigned long, void*);
extern "C" __declspec(dllimport) int __stdcall WriteFile(void*, const void*, unsigned long, unsigned long*, void*);
extern "C" __declspec(dllimport) int __stdcall CloseHandle(void*);
static void mark(const char* text, unsigned long n) {
  void* h = CreateFileA("marca.txt", 0x0004, 0, nullptr, 4, 0x80, nullptr);
  unsigned long w = 0; if (h != (void*)-1) { WriteFile(h, text, n, &w, nullptr); CloseHandle(h); } }
'''


def ex(cwd, label, cmd):
    r = subprocess.run(cmd, cwd=cwd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
    (cwd / (label + '.log')).write_text(repr(cmd) + '\nEXIT ' + str(r.returncode) + '\n' + r.stdout, encoding='utf-8')
    return r.returncode, r.stdout


def err(t):
    lines = [l.strip()[:220] for l in t.splitlines() if 'error' in l]
    return ' | '.join(lines)[:500] if lines else t.strip()[-300:]


# (name, header, source, NativeSharp body, crt startup, dll, extra compile flags)
CASES = [
    ('constructor-global-sin-crt', 'namespace A { int value() noexcept; }',
     'static int compute() { return 7; }\nstatic int x = compute();\nnamespace A { int value() noexcept { return x; } }',
     'return A.value();', False, False, []),
    ('constructor-destructor-sin-crt', 'namespace A { int value() noexcept; }',
     MARK + 'struct G { int v; G() : v(7) { mark("c", 1); } ~G() { mark("d", 1); } };\nstatic G g;\nnamespace A { int value() noexcept { return g.v; } }',
     'return A.value();', False, False, []),
    ('constructor-destructor-crt', 'namespace A { int value() noexcept; }',
     MARK + 'struct G { int v; G() : v(7) { mark("c", 1); } ~G() { mark("d", 1); } };\nstatic G g;\nnamespace A { int value() noexcept { return g.v; } }',
     'return A.value();', True, False, []),
    ('tls-dinamico-hilo', '#include <thread>\nnamespace A { using Cb = int(*)(); int local() noexcept; int inThread(Cb f); }',
     'static int seed = 0;\nstruct T { int v; T() : v(++seed * 10) {} };\nstatic thread_local T t;\n'
     'namespace A { int local() noexcept { return t.v; } int inThread(Cb f) { int r = 0; std::thread th([&] { r = f(); }); th.join(); return r; } }',
     'A.Cb f = () => A.local(); int a = A.local(); int b = A.inThread(f); return a * 100 + b;', True, False, []),
    ('dll-global', '#ifdef T16_EXPORT\n#define API __declspec(dllexport)\n#else\n#define API __declspec(dllimport)\n#endif\nnamespace A { API int value() noexcept; }',
     MARK + 'struct G { int v; G() : v(7) { mark("c", 1); } ~G() { mark("d", 1); } };\nstatic G g;\nnamespace A { int value() noexcept { return g.v; } }',
     'return A.value();', False, True, ['-DT16_EXPORT']),
    ('lto-thin', 'namespace A { int twice(int x) noexcept; }', 'namespace A { int twice(int x) noexcept { return x * 2; } }',
     'return A.twice(21);', False, False, ['-flto=thin']),
    ('duplicado', 'namespace A { int one() noexcept; int two() noexcept; }', 'namespace A { int one() noexcept { return 1; } }',
     'return A.one() + A.two();', False, False, []),
]
for name, header, source, body, crt, dll, flags in CASES:
    if only and name not in only:
        continue
    case = root / name
    case.mkdir()
    (case / 'api.h').write_text('#pragma once\n' + header + '\n', encoding='utf-8')
    (case / 'api.cpp').write_text('#include "api.h"\n' + source + '\n', encoding='utf-8')
    c, t = ex(case, 'clang', [*CXX, *flags, '-c', 'api.cpp', '-o', 'api.obj'])
    if c:
        print(f'{name}: Clang rechaza: {err(t)}', flush=True); continue
    libs = ['api.obj']
    if name == 'duplicado':
        (case / 'b.cpp').write_text('#include "api.h"\nnamespace A { int one() noexcept { return 10; } int two() noexcept { return 2; } }\n',
                                    encoding='utf-8')
        ex(case, 'clang-b', [*CXX, '-c', 'b.cpp', '-o', 'b.obj'])
        ex(case, 'lib-b', ['llvm-lib.exe', '/nologo', '/out:b.lib', 'b.obj'])
    if dll:
        c, t = ex(case, 'dll', ['lld-link.exe', '/nologo', '/dll', '/out:api.dll', '/implib:api.lib', 'api.obj', *[str(p) for p in CRT]])
        if c:
            print(f'{name}: la DLL no enlaza: {err(t)}', flush=True); continue
    else:
        ex(case, 'lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'])
    refs = '<CppLibraryReference Header="api.h" Language="C++" Standard="c++20" Library="api.lib"' + (' Runtime="api.dll"' if dll else '') + ' />'
    if name == 'duplicado':
        refs += '<CppLibraryReference Header="api.h" Language="C++" Standard="c++20" Library="b.lib" />'
    if 'thread' in header:
        refs = refs.replace(' />', '><IncludeDirectory Path="' + str(TS / 'include').replace('\\', '/') + '" Kind="System" />'
                            '<IncludeDirectory Path="' + str(SDK / 'include/ucrt').replace('\\', '/') + '" Kind="System" /></CppLibraryReference>')
    (case / 'Main.ns').write_text('int Main() { ' + body + ' }\n', encoding='utf-8')
    (case / 'App.nsproj').write_text('<Project><PropertyGroup><Name>T16App</Name></PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
                                     + refs + '</ItemGroup></Project>', encoding='utf-8')
    for linker in ('nslink.exe', 'lld-link.exe'):
        if crt and linker == 'nslink.exe':
            continue
        env['NSC_LINKER'] = linker
        cmd = [nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + clang_root, '--no-artifact-cache', '-O0']
        if crt:
            cmd.append('--crt-startup')
            for lib in CRT: cmd += ['--link-lib', str(lib)]
        exe = 'a-' + linker.split('-')[0].split('.')[0] + '.exe'
        c, t = ex(case, 'nsc-' + linker, cmd + ['-o', exe])
        if c:
            print(f'{name} [{linker}]: NSC rechaza: {err(t)}', flush=True); continue
        marker = case / 'marca.txt'
        if marker.exists(): marker.unlink()
        code = ex(case, 'run-' + linker, [str(case / exe)])[0]
        seen = marker.read_text() if marker.exists() else '(sin marca)'
        print(f'{name} [{linker}]: ejecuta {code}; marca {seen!r}', flush=True)
print('FIN', flush=True)
