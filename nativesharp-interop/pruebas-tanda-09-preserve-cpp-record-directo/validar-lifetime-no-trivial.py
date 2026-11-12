"""Compara construccion y destruccion C++ no triviales en un consumidor NS."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
args = parser.parse_args()
root = Path(tempfile.mkdtemp(prefix='nsc-t09-cpp-nontrivial-reg-', dir=r'A:\temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
(root/'api.h').write_text('namespace Life { struct Counted { Counted() noexcept; ~Counted() noexcept; }; int count() noexcept; }\n', encoding='utf-8')
(root/'api.cpp').write_text('#include "api.h"\nnamespace Life { static int value=0; Counted::Counted() noexcept { value+=1; } Counted::~Counted() noexcept { value+=10; } int count() noexcept { return value; } }\n', encoding='utf-8')
(root/'Main.ns').write_text('int Main() { { Life.Counted value = new Life.Counted(); } return Life.count()==11 ? 0 : 1; }\n', encoding='utf-8')
(root/'Control.cpp').write_text('#include "api.h"\nint main() { { Life::Counted value; } return Life::count()==11 ? 0 : 1; }\n', encoding='utf-8')
(root/'Test.nsproj').write_text('<Project><PropertyGroup><Name>Test</Name></PropertyGroup><ItemGroup><Compile Include="Main.ns" /><CppLibraryReference Header="api.h" Language="C++" Standard="c++20" Library="api.lib" /></ItemGroup></Project>', encoding='utf-8')

def run(label, command):
    p = subprocess.run(command, cwd=root, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
    (root/(label+'.log')).write_text('COMMAND: '+repr(command)+'\nEXIT: '+str(p.returncode)+'\n'+p.stdout, encoding='utf-8')
    if p.returncode:
        print(p.stdout, flush=True)
        raise SystemExit(label+': fallo; evidencia: '+str(root))
    return p.stdout

print('Evidencia:', root, flush=True)
for opt in (0, 2):
    suffix = str(opt)
    flag = '-O'+suffix
    run('lib-compile-'+suffix, ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', flag, '-c', str(root/'api.cpp'), '-o', str(root/'api.obj')])
    run('archive-'+suffix, ['llvm-lib.exe', '/nologo', '/out:'+str(root/'api.lib'), str(root/'api.obj')])
    run('control-compile-'+suffix, ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', flag, '-c', str(root/'Control.cpp'), '-o', str(root/'control.obj')])
    run('control-link-'+suffix, ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console', '/nodefaultlib', '/out:'+str(root/'control.exe'), str(root/'control.obj'), str(root/'api.lib')])
    run('control-run-'+suffix, [str(root/'control.exe')])
    run('ns-compile-'+suffix, [args.nsc, '--emit-project-oop-exe', str(root/'Test.nsproj'), '--library-reference-root', str(root), '--cpp-clang-root='+args.clang_root, '--no-artifact-cache', flag, '-o', str(root/'ns.exe')])
    run('ns-run-'+suffix, [str(root/'ns.exe')])
    print(flag+': vida C++ y control correctos', flush=True)
print('OK: vida C++ no trivial; evidencia:', root)
