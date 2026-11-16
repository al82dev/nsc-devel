"""Probes for the batch 19 audit: compile small projects and print nsc's diagnostics and exit code.

Usage: python sonda.py casos-a.py (or casos-b.py, casos-c.py); clang.py runs the Clang controls.
"""
import os, subprocess, sys, tempfile
from pathlib import Path
NSC = 'A:/nsc-devel/build-tanda09/bin/nsc.exe'
root = Path(tempfile.mkdtemp(prefix='nsc-t19-sonda-', dir='A:/temp'))
env = os.environ.copy()
env['NSC_LINKER'] = 'nslink.exe'
env['NSC_WINDOWS_RUNTIME_LIBRARY'] = 'A:/nsc-devel/build-tanda09/generated/NativeSharp.Runtime.Win32.lib'
env['TEMP'] = env['TMP'] = str(root)
env['PATH'] = 'C:/clang-llvm/bin;' + env['PATH']
CASES = {}
SYSTEM = ['A:/vc2026-runtime/toolsets/14.52.36815/include', 'A:/vc2026-runtime/windows-sdk/10.0.28000.0/include/ucrt']
INC = ''.join('<IncludeDirectory Path="' + p + '" Kind="System" />' for p in SYSTEM)
exec(Path(sys.argv[1]).read_text(encoding='utf-8'))
print('Evidencia:', root)
for name, (lang, std, header, main, impl) in CASES.items():
    case = root / name
    case.mkdir()
    (case / 'api.h').write_text(header, encoding='utf-8')
    (case / 'Main.ns').write_text(main, encoding='utf-8')
    lib = ''
    if impl:
        ext = 'c' if lang == 'C' else 'cpp'
        (case / ('api.' + ext)).write_text('#include "api.h"\n' + impl, encoding='utf-8')
        cc = ['clang.exe' if lang == 'C' else 'clang++.exe', '-target', 'x86_64-pc-windows-msvc', '-std=' + std,
              *[x for p in SYSTEM for x in ('-isystem', p)], '-c',
              'api.' + ext, '-o', 'api.obj']
        r = subprocess.run(cc, cwd=case, env=env, capture_output=True, text=True)
        if r.returncode: print(name, 'CC FALLA', r.stderr[-800:]); continue
        subprocess.run(['llvm-lib.exe', '/nologo', '/out:api.lib', 'api.obj'], cwd=case, env=env, check=True)
        lib = ' Library="api.lib"'
    (case / 'App.nsproj').write_text(
        '<Project><PropertyGroup><Name>T19</Name></PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
        f'<CppLibraryReference Header="api.h" Language="{lang}" Standard="{std}"{lib}>' + (INC if '#include <' in header else '') +
        '</CppLibraryReference></ItemGroup></Project>',
        encoding='utf-8')
    r = subprocess.run([NSC, '--emit-project-oop-exe', 'App.nsproj', '--cpp-clang-root=C:/clang-llvm',
                        '--no-artifact-cache', '-o', 'app.exe'], cwd=case, env=env, capture_output=True, text=True,
                       errors='replace')
    out = (r.stdout + r.stderr).strip()
    if r.returncode == 0:
        run = subprocess.run([str(case / 'app.exe')], cwd=case).returncode
        print(f'== {name}: compila; devuelve {run}')
    else:
        print(f'== {name}: nsc {r.returncode}')
        print('\n'.join(l for l in out.splitlines() if 'error' in l or 'note' in l)[:1500])
