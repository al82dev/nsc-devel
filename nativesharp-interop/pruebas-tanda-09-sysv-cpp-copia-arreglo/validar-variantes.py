"""Contrasta arreglos escalares fijos dentro de records C++ por copia."""
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
root=Path(tempfile.mkdtemp(prefix='nsc-t09-copia-arreglo-variantes-',dir=r'A:\temp'))
print('Evidencia:',root,flush=True)
for label in ('float3','short5'):
    dest=root/label
    shutil.copytree(source,dest)
    header=dest/'api.h'
    header.write_text(header.read_text().replace('int x[3];', 'float x[3];' if label=='float3' else 'short x[5];'))
    impl=dest/'api.cpp'
    text=impl.read_text()
    if label=='float3':
        text=text.replace('p.x[2]+3','(int)(p.x[2]+3)')
    else:
        text=text.replace('p.y!=40','p.y!=40 || p.x[3]!=40 || p.x[4]!=50')
        for name in ('Main.ns','Producer.ns','Control.cpp'):
            path=dest/name
            content=path.read_text().replace('s.x[2]=30;', 's.x[2]=30; s.x[3]=40; s.x[4]=50;')
            content=content.replace('s{{10,20,30},40}', 's{{10,20,30,40,50},40}')
            content=content.replace('s.x[2]==30 &&','s.x[2]==30 && s.x[3]==40 && s.x[4]==50 &&')
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
