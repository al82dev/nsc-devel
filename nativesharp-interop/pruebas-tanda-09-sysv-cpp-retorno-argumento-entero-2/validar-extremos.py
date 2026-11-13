"""Contrasta argumentos de uno y cuatro enteros con retorno C++ mixto."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

parser=argparse.ArgumentParser()
parser.add_argument('--nsc',required=True)
parser.add_argument('--clang-root',required=True)
args=parser.parse_args()
source=Path(__file__).resolve().parent
root=Path(tempfile.mkdtemp(prefix='nsc-t09-argumentos-enteros-extremos-',dir=r'A:\temp'))
print('Evidencia:',root,flush=True)
for count in (1,4):
    dest=root/str(count)
    shutil.copytree(source,dest)
    header=dest/'api.h'
    fields='int x;' if count==1 else 'int x,y,z,w;'
    header.write_text(header.read_text().replace('int x,y;',fields))
    impl=dest/'api.cpp'
    result='(float)(p.x+1),(float)(p.x+10),p.x+23' if count==1 else '(float)(p.x+1),(float)(p.y+p.z-30),p.w-7'
    impl.write_text(impl.read_text().replace('(float)(p.x+1),(float)p.y,p.y+13',result))
    for name in ('Main.ns','Producer.ns','Control.cpp'):
        path=dest/name
        text=path.read_text()
        if count==1:
            text=text.replace('s.y=20;','').replace('s{10,20}','s{10}').replace(' && s.y==20','')
        else:
            text=text.replace('s.y=20;','s.y=20; s.z=30; s.w=40;').replace('s{10,20}','s{10,20,30,40}').replace('s.y==20 &&','s.y==20 && s.z==30 && s.w==40 &&')
        path.write_text(text)
    validator=dest/'validar.py'
    if count==1: validator.write_text(validator.read_text().replace(r'\(i64',r'\(i32'))
    for script in ('validar.py','validar-precompilado.py'):
        command=[sys.executable,str(dest/script),'--nsc',args.nsc,'--clang-root',args.clang_root]
        result=subprocess.run(command,env=os.environ.copy(),stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,errors='replace')
        (root/(str(count)+'-'+script+'.log')).write_text('COMMAND: '+repr(command)+'\nEXIT: '+str(result.returncode)+'\n'+result.stdout,encoding='utf-8')
        print(count,script,'EXIT',result.returncode,flush=True)
        if result.returncode:
            print(result.stdout,flush=True)
            raise SystemExit('Fallo; evidencia: '+str(root))
print('OK: extremos; evidencia:',root)
