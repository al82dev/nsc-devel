"""Contrasta copias C++ grandes con campos mixtos o de doble precision."""
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
root=Path(tempfile.mkdtemp(prefix='nsc-t09-retorno-copia-grande-variantes-',dir=r'A:\temp'))
print('Evidencia:',root,flush=True)
for label in ('mixto32','double64'):
    dest=root/label
    shutil.copytree(source,dest)
    header=dest/'api.h'
    fields='float x; int y; float z; int w,v;' if label=='mixto32' else 'double x,y,z;'
    header.write_text(header.read_text().replace('int x,y,z,w,v;',fields))
    impl=dest/'api.cpp'
    text=impl.read_text()
    if label=='mixto32':
        text=text.replace('p.z+3','(int)(p.z+3)')
    else:
        text=text.replace('if (p.w!=40 || p.v!=50) { Small fail{-99,-99,-99}; return fail; } ','')
        text=text.replace('p.z+3','(int)(p.z+3)').replace(' p.w=999;','')
        for name in ('Main.ns','Producer.ns','Control.cpp'):
            path=dest/name
            content=path.read_text().replace('s.w=40; s.v=50;','').replace('s{10,20,30,40,50}','s{10,20,30}').replace(' && s.w==40 && s.v==50','')
            path.write_text(content)
    impl.write_text(text)
    for script in ('validar.py','validar-precompilado.py'):
        command=[sys.executable,str(dest/script),'--nsc',args.nsc,'--clang-root',args.clang_root]
        result=subprocess.run(command,env=os.environ.copy(),stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,errors='replace')
        (root/(label+'-'+script+'.log')).write_text('COMMAND: '+repr(command)+'\nEXIT: '+str(result.returncode)+'\n'+result.stdout,encoding='utf-8')
        print(label,script,'EXIT',result.returncode,flush=True)
        if result.returncode:
            print(result.stdout,flush=True)
            raise SystemExit('Fallo; evidencia: '+str(root))
print('OK: variantes; evidencia:',root)
