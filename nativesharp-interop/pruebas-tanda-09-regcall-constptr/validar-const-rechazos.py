"""Verifica que los alias de función no pierdan la calificación del apuntado."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
parser.add_argument('--cpp', action='store_true')
args = parser.parse_args()
fixture = Path(__file__).resolve().parent
if args.cpp:
    fixture = fixture.parent/'pruebas-tanda-09-regcall-cpp-constptr'
root = Path(tempfile.mkdtemp(prefix='nsc-t09-regcall-const-rechazos-', dir=r'A:\temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)
source_name, project_name = ('api.cpp', 'Probe.nsproj') if args.cpp else ('api.c', 'VectorCall.nsproj')
for name in ('api.h', source_name, project_name):
    shutil.copyfile(fixture/name, root/name)
with (root/'api.h').open('a', encoding='utf-8') as header:
    if args.cpp:
        header.write('\nnamespace T09HfaCpp { using MutableFn=Nested (__attribute__((regcall)) *)(Nested,int*,int*);\n'
                     'MutableFn fetchMutable(); const int* fetchConst(); }\n')
    else:
        header.write('\ntypedef T09Nested (__attribute__((regcall)) *MutableFn)(T09Nested,int*,int*);\n'
                     'MutableFn t09_get_mutable(void);\nconst int* t09_get_const(void);\n')
with (root/source_name).open('a', encoding='utf-8') as source:
    if args.cpp:
        source.write('\nnamespace T09HfaCpp { MutableFn fetchMutable() { return nullptr; }\n'
                     'const int* fetchConst() { static const int x=1; return &x; } }\n')
    else:
        source.write('\nMutableFn t09_get_mutable(void) { return 0; }\n'
                     'const int* t09_get_const(void) { static const int x=1; return &x; }\n')
for label, command in (
    ('compile', ['clang.exe','-target','x86_64-pc-windows-msvc', '-std=c++20' if args.cpp else '-std=c11',
                 '-c',str(root/source_name),'-o',str(root/'api.obj')]),
    ('archive', ['llvm-lib.exe','/nologo','/out:'+str(root/'api.lib'),str(root/'api.obj')]),
):
    result = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
    (root/(label+'.log')).write_text('EXIT: '+str(result.returncode)+'\n'+result.stdout, encoding='utf-8')
    if result.returncode: raise SystemExit(label+': fallo; evidencia: '+str(root))
cases = (
    ('const-a-mutable', 'T09HfaCpp.MutableFn f=T09HfaCpp.fetchNested();' if args.cpp else 'MutableFn f=t09_get_nested();', "cannot initialize local 'f'"),
    ('mutable-a-const', 'T09HfaCpp.NestedFn f=T09HfaCpp.fetchMutable();' if args.cpp else 'T09NestedFn f=t09_get_mutable();', "cannot initialize local 'f'"),
    ('dato-const-a-mutable', 'int* p=T09HfaCpp.fetchConst();' if args.cpp else 'int* p=t09_get_const();', 'would discard pointee constness'),
)
for label, statement, expected in cases:
    (root/'Main.ns').write_text('int Main() { '+statement+' return 0; }', encoding='utf-8')
    command=[args.nsc,'--dump-project-oop-llvm',str(root/project_name),
             '--cpp-clang-root='+args.clang_root,'--library-reference-root',str(root)]
    result=subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
    (root/(label+'.log')).write_text('EXIT: '+str(result.returncode)+'\n'+result.stdout, encoding='utf-8')
    if result.returncode == 0 or expected not in result.stdout:
        raise SystemExit(label+': rechazo esperado ausente; evidencia: '+str(root/(label+'.log')))
    print(label+': rechazo esperado', flush=True)
print('OK: tres conversiones rechazadas; evidencia:', root, flush=True)
