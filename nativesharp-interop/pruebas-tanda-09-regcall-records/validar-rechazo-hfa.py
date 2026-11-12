"""Conserva el rechazo de la clasificacion regcall Expand aun no implementada."""
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
root = Path(tempfile.mkdtemp(prefix='nsc-t09-regcall-hfa-rechazo-', dir=r'A:\temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)
for label in ('directo', 'puntero'):
    case = root / label
    case.mkdir()
    shutil.copyfile(fixture / 'VectorCall.nsproj', case / 'VectorCall.nsproj')
    header = ('typedef struct Hfa { float x,y,z; } Hfa;\n'
              'typedef Hfa (__attribute__((regcall)) *HfaFn)(Hfa,int,long long);\n')
    header += ('Hfa __attribute__((regcall)) t09_hfa(Hfa,int,long long);\n' if label == 'directo'
               else 'HfaFn t09_get_hfa(void);\n')
    (case / 'api.h').write_text(header, encoding='utf-8')
    expr = 't09_hfa(s,1,(long)2)' if label == 'directo' else 'p(s,1,(long)2)'
    local = '' if label == 'directo' else 'HfaFn p=t09_get_hfa(); '
    (case / 'Main.ns').write_text('int Main() { Hfa s; s.x=(float)1; s.y=(float)2; '+local+
                                   'Hfa r='+expr+'; return r.x==(float)1 ? 0 : 1; }\n', encoding='utf-8')
    (case / 'empty.c').write_text('int t09_empty(void) { return 0; }\n', encoding='utf-8')
    for name, command in (
        ('compile', ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-c', str(case/'empty.c'), '-o', str(case/'api.obj')]),
        ('archive', ['llvm-lib.exe', '/nologo', '/out:'+str(case/'api.lib'), str(case/'api.obj')]),
    ):
        p = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
        (case/(name+'.log')).write_text('EXIT: '+str(p.returncode)+'\n'+p.stdout, encoding='utf-8')
        if p.returncode: raise SystemExit(label+': preparacion fallo; evidencia: '+str(case))
    command = [args.nsc, '--dump-project-oop-llvm', str(case/'VectorCall.nsproj'),
               '--cpp-clang-root='+args.clang_root, '--library-reference-root', str(case)]
    p = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
    (case/'nsc.log').write_text('EXIT: '+str(p.returncode)+'\n'+p.stdout, encoding='utf-8')
    expected = ('regcall currently supports only' if label == 'directo'
                else 'regcall function pointer signature is not yet supported')
    if p.returncode == 0 or expected not in p.stdout:
        raise SystemExit(label+': rechazo esperado ausente; evidencia: '+str(case))
    print(label+': clasificacion no representada rechazada', flush=True)
print('OK: rechazos HFA; evidencia:', root)
