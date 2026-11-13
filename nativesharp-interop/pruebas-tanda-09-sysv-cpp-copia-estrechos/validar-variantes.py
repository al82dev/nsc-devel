"""Contrasta campos estrechos con signo inverso en un record C++ por copia."""
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
root=Path(tempfile.mkdtemp(prefix='nsc-t09-copia-estrechos-inversos-',dir=r'A:\temp'))
dest=root/'inversos'
shutil.copytree(source,dest)
header=dest/'api.h'
header.write_text(header.read_text().replace('signed char x; double y; unsigned short z;', 'unsigned char x; double y; short z;'))
for name in ('Main.ns','Producer.ns','Control.cpp'):
    path=dest/name
    text=path.read_text().replace('s.x=-12;', 's.x=250;').replace('s.z=60000;', 's.z=-1234;').replace('s{-12,20,60000}','s{250,20,-1234}')
    text=text.replace('s.x==-12','s.x==250').replace('s.z==60000','s.z==-1234').replace('a.x==-11','a.x==251').replace('b.x==-11','b.x==251').replace('a.z==60003','a.z==-1231').replace('b.z==60003','b.z==-1231')
    path.write_text(text)
print('Evidencia:',root,flush=True)
for script in ('validar.py','validar-precompilado.py'):
    command=[sys.executable,str(dest/script),'--nsc',args.nsc,'--clang-root',args.clang_root]
    result=subprocess.run(command,env=os.environ.copy(),stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,errors='replace')
    (root/(script+'.log')).write_text('COMMAND: '+repr(command)+'\nEXIT: '+str(result.returncode)+'\n'+result.stdout,encoding='utf-8')
    print(script,'EXIT',result.returncode,flush=True)
    if result.returncode:
        print(result.stdout,flush=True)
        raise SystemExit('Fallo; evidencia: '+str(root))
print('OK: signos inversos; evidencia:',root)
