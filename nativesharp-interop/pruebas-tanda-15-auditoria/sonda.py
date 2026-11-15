"""Sonda de auditoria T15 (no es una prueba de soporte): excepciones, callbacks y recursos C++ consumidos
desde NativeSharp.

Cada caso compila una biblioteca C++ con Clang 23.1.0 y un consumidor NativeSharp que devuelve 0 si los
valores coinciden. Imprime: compila y ejecuta (codigo), o el primer diagnostico. Para los casos cuyo
resultado esperado es terminar el proceso, imprime tambien el codigo de un control C++ equivalente.
"""
import os, subprocess, sys, tempfile
from pathlib import Path
nsc, clang_root = sys.argv[1], sys.argv[2]
only = set(sys.argv[3:])
root = Path(tempfile.mkdtemp(prefix='nsc-t15-sonda-', dir='A:/temp'))
env = os.environ.copy(); env['TEMP'] = env['TMP'] = str(root)
env['NSC_LINKER'] = 'nslink.exe'
env['NSC_WINDOWS_RUNTIME_LIBRARY'] = 'A:/nsc-devel/build-tanda09/generated/NativeSharp.Runtime.Win32.lib'
print('Evidencia:', root, flush=True)


def ex(cwd, label, cmd):
    r = subprocess.run(cmd, cwd=cwd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
    (cwd / (label + '.log')).write_text(repr(cmd) + '\nEXIT ' + str(r.returncode) + '\n' + r.stdout, encoding='utf-8')
    return r.returncode, r.stdout


def err(t):
    lines = [l.strip()[:220] for l in t.splitlines() if 'error' in l]
    return ' | '.join(lines)[:500] if lines else t.strip()[-300:]


CB = 'namespace A { using Cb = int(*)(int); using CtxCb = int(*)(void*, int); }\n'
# (name, header, implementation, NativeSharp prelude, NativeSharp body, C++ control main or None)
CASES = [
    ('callback-inmediato', CB + 'namespace A { int apply(Cb f, int x) noexcept; }',
     'namespace A { int apply(Cb f, int x) noexcept { return f(x); } }', '',
     'A.Cb f = (int v) => v * 2; return A.apply(f, 21) == 42 ? 0 : 1;', None),
    ('callback-conservado', CB + 'namespace A { void store(Cb f) noexcept; int callStored(int x) noexcept; }',
     'namespace A { static Cb saved; void store(Cb f) noexcept { saved = f; } int callStored(int x) noexcept { return saved(x); } }', '',
     'A.Cb f = (int v) => v + 1; A.store(f); return A.callStored(41) == 42 ? 0 : 1;', None),
    ('callback-con-contexto', CB + 'namespace A { int applyCtx(CtxCb f, void* ctx, int x) noexcept; }',
     'namespace A { int applyCtx(CtxCb f, void* ctx, int x) noexcept { return f(ctx, x); } }',
     'struct P { public int k; }\n',
     'P p; p.k = 40; A.CtxCb f = (void* c, int v) => ((P*)c)->k + v; return A.applyCtx(f, &p, 2) == 42 ? 0 : 1;', None),
    ('lambda-con-captura', CB + 'namespace A { int apply(Cb f, int x) noexcept; }',
     'namespace A { int apply(Cb f, int x) noexcept { return f(x); } }', '',
     'int k = 40; A.Cb f = (int v) => v + k; return A.apply(f, 2) == 42 ? 0 : 1;', None),
    ('excepcion-cruza-callback', CB + 'namespace A { int thrower(int x); int guarded(Cb f); }',
     'namespace A { int thrower(int x) { throw 40 + x; } int guarded(Cb f) { try { return f(1); } catch (int e) { return e; } } }', '',
     'A.Cb f = (int v) => A.thrower(v); return A.guarded(f) == 41 ? 0 : 1;', None),
    ('finally-durante-desenrollado', CB + 'namespace A { int thrower(int x); int guarded(Cb f); }',
     'namespace A { int thrower(int x) { throw 40 + x; } int guarded(Cb f) { try { return f(1); } catch (int e) { return e; } } }',
     'int counter = 0;\nint Inner(int v) { try { return A.thrower(v); } finally { counter = counter + 1; } }\n',
     'A.Cb f = (int v) => Inner(v); int r = A.guarded(f); return r == 41 && counter == 1 ? 0 : 1;', None),
    ('excepcion-sin-catch', 'namespace A { int thrower(int x); }', 'namespace A { int thrower(int x) { throw 40 + x; } }', '',
     'return A.thrower(1);', 'int main() { return A::thrower(1); }'),
    ('catch-nativesharp', 'namespace A { int thrower(int x); }', 'namespace A { int thrower(int x) { throw 40 + x; } }', '',
     'try { return A.thrower(1); } catch (NativeSharp.Core.Exception e) { return 0; }', None),
    ('noexcept-lanza', 'namespace A { int bad(int x) noexcept; }',
     'namespace A { int raise(int x) { throw x; } int bad(int x) noexcept { return raise(x); } }', '',
     'return A.bad(1);', 'int main() { return A::bad(1); }'),
    ('movible-no-copiable',
     'namespace A { struct Handle { int* p; explicit Handle(int v); Handle(Handle&& o) noexcept; Handle(const Handle&) = delete; ~Handle(); }; Handle make(int v); int consume(Handle h); }',
     'namespace A { static int buffer[4]; static int next = 0; Handle::Handle(int v) : p(&buffer[next++ % 4]) { *p = v; } Handle::Handle(Handle&& o) noexcept : p(o.p) { o.p = nullptr; } Handle::~Handle() { if (p) *p = -1; } Handle make(int v) { return Handle(v); } int consume(Handle h) { return *h.p; } }',
     '', 'A.Handle h = A.make(42); return A.consume(h) == 42 ? 0 : 1;', None),
    ('smart-pointer', '#include <memory>\nnamespace A { std::unique_ptr<int> make(int v); int read(const std::unique_ptr<int>& p) noexcept; }',
     'namespace A { std::unique_ptr<int> make(int v) { return std::make_unique<int>(v); } int read(const std::unique_ptr<int>& p) noexcept { return *p; } }',
     '', 'return A.read(A.make(42)) == 42 ? 0 : 1;', None),
    ('allocator', '#include <memory>\nnamespace A { int viaAllocator(int v); }',
     'namespace A { int viaAllocator(int v) { std::allocator<int> a; int* p = a.allocate(1); *p = v; int r = *p; a.deallocate(p, 1); return r; } }',
     '', 'return A.viaAllocator(42) == 42 ? 0 : 1;', None),
    ('corutina', '#include <coroutine>\nnamespace A { struct Gen { struct promise_type { int v = 0; Gen get_return_object() { return Gen{std::coroutine_handle<promise_type>::from_promise(*this)}; } std::suspend_always initial_suspend() noexcept { return {}; } std::suspend_always final_suspend() noexcept { return {}; } std::suspend_always yield_value(int x) noexcept { v = x; return {}; } void return_void() noexcept {} void unhandled_exception() noexcept {} }; std::coroutine_handle<promise_type> h; }; Gen counter(int n); int next(Gen& g) noexcept; void destroy(Gen& g) noexcept; }',
     'namespace A { Gen counter(int n) { for (int i = 0; i < n; ++i) co_yield i; } int next(Gen& g) noexcept { g.h.resume(); return g.h.promise().v; } void destroy(Gen& g) noexcept { g.h.destroy(); } }',
     '', 'A.Gen g = A.counter(5); A.next(ref g); int r = A.next(ref g); A.destroy(ref g); return r == 1 ? 0 : 1;', None),
]

for name, header, impl, prelude, body, control in CASES:
    if only and name not in only:
        continue
    case = root / name
    case.mkdir()
    (case / 'api.h').write_text('#pragma once\n' + header + '\n', encoding='utf-8')
    (case / 'api.cpp').write_text('#include "api.h"\n' + impl + '\n', encoding='utf-8')
    c, t = ex(case, 'clang', ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-c', 'api.cpp', '-o', 'api.obj'])
    if c:
        print(f'{name}: Clang rechaza: {err(t)}', flush=True)
        continue
    ex(case, 'lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'])
    control_code = None
    if control:
        (case / 'Control.cpp').write_text('#include "api.h"\n' + control + '\n', encoding='utf-8')
        c, t = ex(case, 'control', ['clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', 'Control.cpp', 'api.lib',
                                    '-o', 'control.exe'])
        control_code = ex(case, 'control-run', [str(case / 'control.exe')])[0] if c == 0 else 'no enlaza: ' + err(t)
    (case / 'Main.ns').write_text(prelude + 'int Main() { ' + body + ' }\n', encoding='utf-8')
    (case / 'App.nsproj').write_text('<Project><PropertyGroup><Name>T15App</Name></PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
        '<CppLibraryReference Header="api.h" Language="C++" Standard="c++20" Library="api.lib" /></ItemGroup></Project>',
        encoding='utf-8')
    c, t = ex(case, 'nsc', [nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + clang_root,
                            '--no-artifact-cache', '-O0', '-o', 'a.exe'])
    if c:
        print(f'{name}: NSC rechaza: {err(t)}', flush=True)
        continue
    code = ex(case, 'run', [str(case / 'a.exe')])[0]
    print(f'{name}: ejecuta {code}' + (f' (control C++: {control_code})' if control else ''), flush=True)
print('FIN', flush=True)
