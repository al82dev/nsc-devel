"""Comprueba el rechazo de usos por valor de records C++ no admitidos."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
args = parser.parse_args()
fixture = Path(__file__).resolve().parent
root = Path(tempfile.mkdtemp(prefix='nsc-t09-cpp-record-ptr-rechazos-', dir=r'A:\temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)
cases = {
    'copia-eliminada': 'Small(const Small&) = delete; int x,y;',
    'copia-privada': 'private: Small(const Small&) = default; public: int x,y;',
    'constructor-eliminado': 'Small() = delete; int x,y;',
    'constructor-privado': 'private: Small() = default; public: int x,y;',
    'destructor-no-trivial': '~Small(); int x,y;',
    'destructor-privado': 'private: ~Small() = default; public: int x,y;',
    'asignacion-eliminada': 'int x,y; Small& operator=(const Small&) = delete;',
    'asignacion-privada': 'int x,y; private: Small& operator=(const Small&) = default;',
    'miembro-volatile': 'volatile int x; int y;',
    'volatile-anidado': 'struct Inner { volatile int z; }; int x,y; Inner inner;',
}
for label, members in cases.items():
    case = root / label
    case.mkdir()
    shutil.copyfile(fixture / 'VectorCall.nsproj', case / 'VectorCall.nsproj')
    shutil.copyfile(fixture / 'Main.ns', case / 'Main.ns')
    (case / 'api.h').write_text('namespace T09Cpp { struct Small { '+members+' }; '
        'using Callback = Small (__attribute__((sysv_abi)) *)(Small); '
        'Small __attribute__((sysv_abi)) most(Small); Callback fetchCallback(); }\n', encoding='utf-8')
    (case / 'empty.c').write_text('int t09_dummy(void) { return 0; }\n', encoding='utf-8')
    for name, command in (
        ('compile', ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-c', str(case/'empty.c'), '-o', str(case/'empty.obj')]),
        ('archive', ['llvm-lib.exe', '/nologo', '/out:'+str(case/'api.lib'), str(case/'empty.obj')]),
    ):
        p = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
        (case/(name+'.log')).write_text('EXIT: '+str(p.returncode)+'\n'+p.stdout, encoding='utf-8')
        if p.returncode: raise SystemExit(label+': fallo de preparacion; evidencia: '+str(case))
    command = [args.nsc, '--dump-project-oop-llvm', str(case/'VectorCall.nsproj'),
               '--cpp-clang-root='+args.clang_root, '--library-reference-root', str(case)]
    p = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
    (case/'nsc.log').write_text('EXIT: '+str(p.returncode)+'\n'+p.stdout, encoding='utf-8')
    if p.returncode == 0 or 'error[NS3020]' not in p.stdout or 'move-only argument cannot be copied into a by-value delegate parameter' not in p.stdout:
        raise SystemExit(label+': falta el rechazo esperado del argumento indirecto; evidencia: '+str(case))
    print(label+': rechazado', flush=True)
print('OK: diez tipos no admitidos rechazados; evidencia:', root)
