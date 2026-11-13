"""Contrasta campos puntero a float y double dentro de la copia C++."""
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
root=Path(tempfile.mkdtemp(prefix='nsc-t09-copia-punteros-variantes-',dir=r'A:\temp'))
print('Evidencia:',root,flush=True)
for kind in ('float','double'):
    dest=root/kind
    shutil.copytree(source,dest)
    header=dest/'api.h'
    header.write_text(header.read_text().replace('int* z;',kind+'* z;'))
    impl=dest/'api.cpp'
    impl.write_text(impl.read_text().replace('*p.z+3','(int)*p.z+3'))
    for name in ('Main.ns','Producer.ns','Control.cpp'):
        path=dest/name
        path.write_text(path.read_text().replace('int value=30;',kind+' value=30;'))
    for script in ('validar.py','validar-precompilado.py'):
        command=[sys.executable,str(dest/script),'--nsc',args.nsc,'--clang-root',args.clang_root]
        result=subprocess.run(command,env=os.environ.copy(),stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,errors='replace')
        (root/(kind+'-'+script+'.log')).write_text('COMMAND: '+repr(command)+'\nEXIT: '+str(result.returncode)+'\n'+result.stdout,encoding='utf-8')
        print(kind,script,'EXIT',result.returncode,flush=True)
        if result.returncode:
            print(result.stdout,flush=True)
            raise SystemExit('Fallo; evidencia: '+str(root))
print('OK: variantes; evidencia:',root)
