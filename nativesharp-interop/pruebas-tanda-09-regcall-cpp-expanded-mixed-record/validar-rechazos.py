"""Comprueba que el registro compacto conserva las restricciones de C++."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

parser=argparse.ArgumentParser()
parser.add_argument('--nsc',required=True)
parser.add_argument('--clang-root',required=True)
args=parser.parse_args()
fixture=Path(__file__).resolve().parent
root=Path(tempfile.mkdtemp(prefix='nsc-t09-cpp-expanded-mixed-rechazos-',dir=r'A:\temp'))
env=dict(os.environ,TEMP=str(root),TMP=str(root))
print('Evidencia:',root,flush=True)
for name in ('api.h','api.cpp','Probe.nsproj'):
    shutil.copyfile(fixture/name,root/name)
for label,cmd in (
    ('compile',['clang.exe','-target','x86_64-pc-windows-msvc','-std=c++20','-c',str(root/'api.cpp'),'-o',str(root/'api.obj')]),
    ('archive',['llvm-lib.exe','/nologo','/out:'+str(root/'api.lib'),str(root/'api.obj')])):
    r=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,errors='replace')
    (root/(label+'.log')).write_text('COMMAND: '+repr(cmd)+'\nEXIT: '+str(r.returncode)+'\n'+r.stdout,encoding='utf-8')
    if r.returncode: raise SystemExit(label+': fallo; evidencia: '+str(root))
original=(root/'api.h').read_text(encoding='utf-8')
for label,declaration,expected in (('privado','struct OtherNested { float x; private: int y; };',"has no compatible member named"),
                          ('no-trivial','struct OtherNested { float x; int y; ~OtherNested(); };','experimental C++ local requires direct automatic construction and cleanup')):
    (root/'api.h').write_text(original.replace('struct OtherNested { float x; int y; };',declaration),encoding='utf-8')
    (root/'Main.ns').write_text('int Main() { T09HfaCpp.OtherNested n; n.x=(float)1; n.y=2; return 0; }',encoding='utf-8')
    cmd=[args.nsc,'--dump-project-oop-llvm',str(root/'Probe.nsproj'),'--cpp-clang-root='+args.clang_root,'--library-reference-root',str(root)]
    r=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,errors='replace')
    (root/(label+'.log')).write_text('COMMAND: '+repr(cmd)+'\nEXIT: '+str(r.returncode)+'\n'+r.stdout,encoding='utf-8')
    if r.returncode==0 or expected not in r.stdout:
        raise SystemExit(label+': no produjo el rechazo esperado; evidencia: '+str(root))
print('OK: record compacto privado y no trivial rechazados; evidencia:',root,flush=True)
