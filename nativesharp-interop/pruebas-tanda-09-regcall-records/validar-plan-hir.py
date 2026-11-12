"""Comprueba el transporte HIR/MIR de planes regcall para punteros C vivos."""
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
root = Path(tempfile.mkdtemp(prefix='nsc-t09-regcall-record-plan-hir-', dir=r'A:\temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)
for name in ('api.h', 'api.c', 'VectorCall.nsproj'):
    shutil.copyfile(fixture / name, root / name)
header = root / 'api.h'
header.write_text('\n'.join(line for line in header.read_text(encoding='utf-8').splitlines()
    if not (line.startswith('API_STORAGE ') and '__attribute__((regcall))' in line)) + '\n', encoding='utf-8')
(root / 'Main.ns').write_text(
    'int Main() { T09Small s; s.x=10; s.y=20; T09Large l; l.a=1; l.b=2; l.c=3; l.d=4; l.e=5; l.f=6; '
    'MostPtr p=t09_get_most(); MostVoid q=t09_get_most_void(); T09Small a=p(s); T09Large b=q(l); '
    'return a.x==11 && b.a==3 ? 0 : 1; }\n', encoding='utf-8')
for label, command in (
    ('compile', ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-c', str(root/'api.c'), '-o', str(root/'api.obj')]),
    ('archive', ['llvm-lib.exe', '/nologo', '/out:'+str(root/'api.lib'), str(root/'api.obj')]),
):
    p = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
    (root/(label+'.log')).write_text('EXIT: '+str(p.returncode)+'\n'+p.stdout, encoding='utf-8')
    if p.returncode: raise SystemExit(label+': fallo; evidencia: '+str(root))
for action in ('hir', 'mir'):
    command = [args.nsc, '--dump-project-oop-'+action, str(root/'VectorCall.nsproj'),
               '--cpp-clang-root='+args.clang_root, '--library-reference-root', str(root)]
    p = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
    (root/(action+'.log')).write_text('EXIT: '+str(p.returncode)+'\n'+p.stdout, encoding='utf-8')
    if p.returncode or 'cpp-pointer-plan=__regcall3____nsc_pointer_abi_probe:1' not in p.stdout or \
            'cpp-pointer-plan=__regcall3____nsc_pointer_abi_probe:2' not in p.stdout:
        raise SystemExit(action+': faltan planes esperados; evidencia: '+str(root))
    print(action+': ambos planes transportados', flush=True)
print('OK: planes HIR/MIR; evidencia:', root)
