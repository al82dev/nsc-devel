"""Compara el acceso TLS de NativeSharp a una global C _Thread_local y a una C++ constinit thread_local."""
import os, subprocess, sys, tempfile
from pathlib import Path
nsc, clang_root = sys.argv[1], sys.argv[2]
root = Path(tempfile.mkdtemp(prefix='nsc-sonda-tls-', dir='A:/temp'))
env = os.environ.copy(); env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)
CRT = 'A:/vc2026-runtime/lib/x64/release'
SDK = 'A:/vc2026-runtime/windows-sdk/10.0.28000.0/lib'
SETS = {'cmt': [f'{CRT}/libcmt.lib'],
        'cuatro': [f'{CRT}/libcmt.lib', f'{CRT}/libcpmt.lib', f'{CRT}/libvcruntime.lib', f'{SDK}/ucrt/x64/libucrt.lib']}
CASES = [
    ('c', 'C', 'c11', 'api.c', '#pragma once\nextern _Thread_local int ctls;\nint ctlsRead(void);\n',
     '#include "api.h"\n_Thread_local int ctls = 7;\nint ctlsRead(void) { return ctls; }\n',
     'return ctls * 100 + ctlsRead();'),
    ('cpp', 'C++', 'c++20', 'api.cpp', '#pragma once\nnamespace T { extern constinit thread_local int tls; int tlsRead(); }\n',
     '#include "api.h"\nnamespace T { constinit thread_local int tls = 7; int tlsRead() { return tls; } }\n',
     'return T.tls * 100 + T.tlsRead();'),
]
def run(cmd, cwd):
    r = subprocess.run(cmd, cwd=cwd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
    (cwd / 'log.txt').open('a', encoding='utf-8').write(repr(cmd) + '\nEXIT ' + str(r.returncode) + '\n' + r.stdout + '\n')
    return r
for name, lang, std, src, header, impl, body in CASES:
    for label, libs in SETS.items():
        case = root / f'{name}-{label}'
        case.mkdir()
        (case / 'api.h').write_text(header, encoding='utf-8')
        (case / src).write_text(impl, encoding='utf-8')
        (case / 'crt.h').write_text('#pragma once\n', encoding='utf-8')
        compiler = 'clang.exe' if lang == 'C' else 'clang++.exe'
        run([compiler, '-target', 'x86_64-pc-windows-msvc', '-std=' + std, '-c', src, '-o', 'api.obj'], case)
        run(['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], case)
        (case / 'Main.ns').write_text('int Main() { ' + body + ' }\n', encoding='utf-8')
        refs = f'<CppLibraryReference Header="api.h" Language="{lang}" Standard="{std}" Library="api.lib" />' + ''.join(
            f'<CppLibraryReference Header="crt.h" Language="C" Standard="c11" Library="{lib}" />' for lib in libs)
        (case / 'App.nsproj').write_text('<Project><PropertyGroup><Name>TlsApp</Name></PropertyGroup><ItemGroup>'
                                         '<Compile Include="Main.ns" />' + refs + '</ItemGroup></Project>', encoding='utf-8')
        r = run([nsc, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=' + clang_root, '--no-artifact-cache',
                 '-O0', '-o', 'a.exe'], case)
        if r.returncode:
            print(case.name, 'rechaza:', ' | '.join(l.strip()[:160] for l in r.stdout.splitlines() if 'error' in l)[:300], flush=True)
            continue
        code = subprocess.run([str(case / 'a.exe')], cwd=case).returncode
        print(case.name, '=>', code if code < 2**31 else code - 2**32, '(esperado 707)', flush=True)
