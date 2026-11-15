"""Sonda de auditoria T14 (no es una prueba de soporte): preprocesador, declaraciones especiales y modulos
C/C++ consumidos desde NativeSharp.

Cada caso compila una biblioteca con Clang 23.1.0 y un consumidor NativeSharp que devuelve 0 si los
valores coinciden. Imprime: compila y ejecuta (codigo), o el primer diagnostico de NSC.
"""
import os, subprocess, sys, tempfile
from pathlib import Path
nsc, clang_root = sys.argv[1], sys.argv[2]
only = set(sys.argv[3:])
root = Path(tempfile.mkdtemp(prefix='nsc-t14-sonda-', dir='A:/temp'))
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
    return ' | '.join(lines)[:400] if lines else t.strip()[-300:]


# (name, language, header, implementation, NativeSharp body, defines, extra header files, rebuild-with-change)
CASES = [
    ('macro-entera', 'C++', '#define T14_N 4\nnamespace A { inline int one() noexcept { return 1; } }', '',
     'return T14_N == 4 ? 0 : 1;', [], {}, None),
    ('macro-expresion', 'C++', '#define T14_SHIFT 3\n#define T14_MASK (1u << T14_SHIFT)\nnamespace A { inline int one() noexcept { return 1; } }', '',
     'return T14_MASK == 8u ? 0 : 1;', [], {}, None),
    ('macro-flotante', 'C++', '#define T14_PI 3.25\nnamespace A { inline int one() noexcept { return 1; } }', '',
     'return T14_PI == 3.25 ? 0 : 1;', [], {}, None),
    ('macro-c', 'C', '#define T14_C 9\nint c_one(void);', 'int c_one(void) { return 1; }',
     'return T14_C == 9 ? 0 : 1;', [], {}, None),
    ('macro-funcion', 'C++', '#define T14_SQ(x) ((x) * (x))\nnamespace A { inline int one() noexcept { return 1; } }', '',
     'return T14_SQ(3) == 9 ? 0 : 1;', [], {}, None),
    ('condicion-configuracion', 'C++',
     'namespace A {\n#ifdef T14_FEATURE\ninline int mode() noexcept { return 1; }\n#else\ninline int mode() noexcept { return 2; }\n#endif\n}', '',
     'return A.mode() == 1 ? 0 : 1;', ['T14_FEATURE=1'], {}, None),
    ('bitfield-cpp', 'C++',
     'namespace A { struct Flags { unsigned a : 3; unsigned b : 5; int c; }; inline int read(const Flags* f) noexcept { return (int)(f->a + f->b) + f->c; } }', '',
     'A.Flags f; f.a = 5; f.b = 9; f.c = 1; return A.read(&f) == 15 ? 0 : 1;', [], {}, None),
    ('bitfield-c', 'C',
     'struct CFlags { unsigned a : 3; unsigned b : 5; int c; };\nint c_read(const struct CFlags* f);',
     'int c_read(const struct CFlags* f) { return (int)(f->a + f->b) + f->c; }',
     'CFlags f; f.a = 5; f.b = 9; f.c = 1; return c_read(&f) == 15 ? 0 : 1;', [], {}, None),
    ('puntero-a-miembro', 'C++',
     'namespace A { struct S { int x; int y; }; inline int S::* pickY() noexcept { return &S::y; } inline int get(const S* s, int S::* m) noexcept { return s->*m; } }', '',
     'A.S s; s.x = 1; s.y = 7; return A.get(&s, A.pickY()) == 7 ? 0 : 1;', [], {}, None),
    ('atributo-packed', 'C++',
     'namespace A { struct [[gnu::packed]] Pk { char a; int b; }; inline int sum(const Pk* p) noexcept { return p->a + p->b; } }', '',
     'A.Pk p; p.a = (sbyte)1; p.b = 2; return A.sum(&p) == 3 && sizeof(A.Pk) == 5 ? 0 : 1;', [], {}, None),
    ('atributo-alineacion', 'C++',
     'namespace A { struct alignas(16) Al { int a; }; inline int get(const Al* p) noexcept { return p->a; } }', '',
     'A.Al p; p.a = 4; return A.get(&p) == 4 && sizeof(A.Al) == 16 ? 0 : 1;', [], {}, None),
    ('no-unique-address', 'C++',
     'namespace A { struct E {}; struct NU { [[no_unique_address]] E e; int x; }; inline int get(const NU* p) noexcept { return p->x; } }', '',
     'A.NU p; p.x = 4; return A.get(&p) == 4 && sizeof(A.NU) == 4 ? 0 : 1;', [], {}, None),
    ('modulo-cpp', 'C++', 'import std;\nnamespace A { inline int one() noexcept { return 1; } }', '',
     'return A.one() == 1 ? 0 : 1;', [], {}, None),
    ('dependencia-transitiva', 'C++', '#include "inner.h"\nnamespace A { inline int value() noexcept { return T14_INNER; } }', '',
     'return A.value();', [], {'inner.h': '#define T14_INNER 3\n'}, ('inner.h', '#define T14_INNER 5\n', 5)),
]

for name, language, header, impl, body, defines, extra, change in CASES:
    if only and name not in only:
        continue
    case = root / name
    case.mkdir()
    (case / 'api.h').write_text('#pragma once\n' + header + '\n', encoding='utf-8')
    for file, text in extra.items():
        (case / file).write_text(text, encoding='utf-8')
    cpp = language == 'C++'
    source = 'api.cpp' if cpp else 'api.c'
    (case / source).write_text('#include "api.h"\n' + impl + '\n', encoding='utf-8')
    compiler = ['clang++.exe', '-std=c++20'] if cpp else ['clang.exe', '-std=c11']
    c, t = ex(case, 'clang', [*compiler, '-target', 'x86_64-pc-windows-msvc', *['-D' + d for d in defines],
                              '-c', source, '-o', 'api.obj'])
    if c:
        print(f'{name}: Clang rechaza: {err(t)}', flush=True)
        continue
    ex(case, 'lib', ['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'])
    (case / 'Main.ns').write_text('int Main() { ' + body + ' }\n', encoding='utf-8')
    standard = 'c++20' if cpp else 'c11'
    children = ''.join('<Define Value="' + d + '" />' for d in defines)
    reference = ('<CppLibraryReference Header="api.h" Language="' + language + '" Standard="' + standard +
                 '" Library="api.lib"' + ('>' + children + '</CppLibraryReference>' if children else ' />'))
    (case / 'App.nsproj').write_text('<Project><PropertyGroup><Name>T14App</Name></PropertyGroup><ItemGroup>'
        '<Compile Include="Main.ns" />' + reference + '</ItemGroup></Project>', encoding='utf-8')
    cache = [] if change else ['--no-artifact-cache']
    c, t = ex(case, 'nsc', [nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + clang_root,
                            *cache, '-O0', '-o', 'a.exe'])
    if c:
        print(f'{name}: NSC rechaza: {err(t)}', flush=True)
        continue
    code = ex(case, 'run', [str(case / 'a.exe')])[0]
    if change:
        file, text, expected = change
        first = code
        (case / file).write_text(text, encoding='utf-8')
        c, t = ex(case, 'nsc2', [nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + clang_root,
                                 '-O0', '-o', 'a.exe'])
        if c:
            print(f'{name}: NSC rechaza al recompilar: {err(t)}', flush=True)
            continue
        code = ex(case, 'run2', [str(case / 'a.exe')])[0]
        print(f'{name}: primera {first}, tras el cambio {code} (se espera {expected})', flush=True)
        continue
    print(f'{name}: ejecuta {code}', flush=True)
print('FIN', flush=True)
