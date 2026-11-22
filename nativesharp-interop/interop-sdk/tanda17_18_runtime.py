"""Tandas 17 y 18: runtime C++ de Microsoft, varias bibliotecas por referencia y Windows SDK (17.x, 18.1, 18.8,
18.14, 18.15).

- Una biblioteca C++ compilada con excepciones y la STL de MSVC (`/MD`) se usa desde NativeSharp sin lanzar (17, 18.1).
- Una referencia C con dos `.lib`, `<Library Path=... />` (18.8).
- WindowsSdkIncludeRoot y VcToolsIncludeRoot: los headers del UCRT se analizan (18.14) y un SDK ausente es un error
  claro (18.15).
Requiere el sysroot local A:/vc2026-runtime; si no existe, la prueba se omite.
"""
from pathlib import Path
import comun

T = 'A:/vc2026-runtime/toolsets/14.52.36815'
K = 'A:/vc2026-runtime/windows-sdk/10.0.28000.0'
PROPS = f'<WindowsSdkIncludeRoot>{K}/include</WindowsSdkIncludeRoot><VcToolsIncludeRoot>{T}/include</VcToolsIncludeRoot>'
RUNTIME = [T + '/lib/x64/msvcprt.lib', T + '/lib/x64/vcruntime.lib', T + '/lib/x64/msvcrt.lib',
           K + '/lib/ucrt/x64/ucrt.lib', K + '/lib/um/x64/kernel32.lib']


def build(base, sources, language):
    for name, text in sources.items():
        (base / name).write_text(text)
        flags = ['-std=c++20', '-fexceptions', '-fcxx-exceptions'] if language == 'C++' else []
        obj = Path(name).with_suffix('.obj').name
        r = comun.run([comun.CLANG, '-c', '--target=x86_64-pc-windows-msvc', *flags, '-isystem', T + '/include',
                       '-isystem', K + '/include/ucrt', name, '-o', obj], base)
        if r.returncode:
            raise RuntimeError((r.stdout or '') + (r.stderr or ''))
        comun.run([comun.LLVM_LIB, '/nologo', f'/out:{Path(name).stem}.lib', obj], base)


def check(label, ok, evidence):
    if ok:
        print(f'ok {label}', flush=True)
    else:
        comun.FAILURES.append(label)
        print(f'FALLA {label}: {evidence[-800:]}', flush=True)


def caso(name, opt, header, sources, language, standard, libs, body, props=PROPS, expect_error=None):
    base = comun.ROOT / name / f'O{opt}'
    base.mkdir(parents=True)
    (base / 'api.h').write_text(header)
    build(base, sources, language)
    first, *rest = libs
    extra = ''.join(f'<Library Path="{p}" />' for p in rest)
    (base / 'Main.ns').write_text('int Main() { ' + body + ' }\n')
    (base / 'App.nsproj').write_text(
        f'<Project><PropertyGroup><Name>App</Name>{props}</PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
        f'<CppLibraryReference Header="api.h" Language="{language}" Standard="{standard}" Library="{first}">{extra}'
        f'</CppLibraryReference>{comun.core(opt)}</ItemGroup></Project>')
    r = comun.nsc(base, 'App', opt, 'exe', 'app.exe', 'nsc')
    label = f'{name}-O{opt}'
    if expect_error:
        check(label, r.returncode != 0 and expect_error in (r.stdout or '') + (r.stderr or ''), (r.stdout or '') + (r.stderr or ''))
        return
    code = comun.run([base / 'app.exe'], base).returncode if r.returncode == 0 else None
    check(label, code == 42, (r.stdout or '') + (r.stderr or '') + f'\nexit={code}')


if not Path(T).is_dir() or not Path(K).is_dir():
    print('omitida: falta A:/vc2026-runtime')
else:
    for opt in comun.OPTS:
        caso('t17-msvc-stl', opt, '#include <string_view>\nint may_throw(int x);\n',
             {'a.cpp': '#include <stdexcept>\nint may_throw(int x) { if (x < 0) throw std::runtime_error("neg"); '
                       'return x; }\n'}, 'C++', 'c++20', ['a.lib'] + RUNTIME, 'return may_throw(42);')
        caso('t18-dos-lib', opt, 'int part_a(void);\nint part_b(void);\n',
             {'a.c': 'int part_a(void) { return 40; }\n', 'b.c': 'int part_b(void) { return 2; }\n'}, 'C', 'c17',
             ['a.lib', 'b.lib'], 'return part_a() + part_b();', props='')
        caso('t18-sdk-ucrt', opt, '#include <stdint.h>\n#include <wchar.h>\nint32_t add32(int32_t a, int32_t b);\n',
             {'a.c': '#include <stdint.h>\nint32_t add32(int32_t a, int32_t b) { return a + b; }\n'}, 'C', 'c17',
             ['a.lib'], 'return add32(40, 2);')
    caso('t18-sdk-ausente', 0, 'int f(void);\n', {'a.c': 'int f(void) { return 0; }\n'}, 'C', 'c17', ['a.lib'],
         'return f();', props='<WindowsSdkIncludeRoot>A:/no/such/sdk</WindowsSdkIncludeRoot>',
         expect_error='WindowsSdkIncludeRoot must be the Include')
comun.fin()
