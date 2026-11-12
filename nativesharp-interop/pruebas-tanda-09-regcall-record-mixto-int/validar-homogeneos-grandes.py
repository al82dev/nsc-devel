"""Contrasta registros homogéneos de cinco float o double pasados por copia."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
parser.add_argument('--solo-double', action='store_true')
args = parser.parse_args()
here = Path(__file__).resolve().parent
root = Path(tempfile.mkdtemp(prefix='nsc-t09-homogeneos-grandes-', dir=r'A:\temp'))
print('Evidencia:', root, flush=True)
for typ in (('double',) if args.solo_double else ('float', 'double')):
    for language, source in (('c', here), ('cpp', here.parent/'pruebas-tanda-09-regcall-cpp-record-mixto-int')):
        target = root / (typ+'-'+language)
        shutil.copytree(source, target, ignore=shutil.ignore_patterns('__pycache__'))
        for file in target.iterdir():
            if not file.is_file():
                continue
            text = file.read_text(encoding='utf-8')
            if file.name == 'api.h':
                text = text.replace('float x; double y;', typ+' x, y, z, w, v;')
            elif file.name in ('api.c', 'api.cpp'):
                text = text.replace('value.x + value.y*2 + bias', 'value.x + value.y*2 + value.z*3 + value.w*4 + value.v*5 + bias')
            elif file.name in ('Control.c', 'Control.cpp'):
                text = text.replace('{10.25,20.5}', '{1,2,3,4,5}').replace('==58', '==62').replace('value.x==10.25', 'value.x==1')
                if typ == 'double':
                    linkage = 'extern "C" ' if language == 'cpp' else ''
                    helpers = linkage + '__attribute__((optnone,noinline)) void* memcpy(void* d, const void* s, unsigned long long n) { unsigned char* a=(unsigned char*)d; const unsigned char* b=(const unsigned char*)s; for(unsigned long long i=0;i<n;++i) a[i]=b[i]; return d; }\n'
                    helpers += linkage + '__attribute__((optnone,noinline)) void* memset(void* d, int c, unsigned long long n) { unsigned char* a=(unsigned char*)d; for(unsigned long long i=0;i<n;++i) a[i]=(unsigned char)c; return d; }\n'
                    text = text.replace('#include "api.h"', '#include "api.h"\n'+helpers)
            elif file.suffix == '.ns':
                text = text.replace('value.x=(float)10.25; value.y=(double)20.5;',
                    'value.x=('+typ+')1; value.y=('+typ+')2; value.z=('+typ+')3; value.w=('+typ+')4; value.v=('+typ+')5;')
                text = text.replace('==58', '==62').replace('value.x==(float)10.25', 'value.x==('+typ+')1')
            file.write_text(text, encoding='utf-8')
        for mode, script, extra in (('directa', 'validar.py', []), ('precompilada-puntero', 'validar-precompilado.py', ['--solo-puntero'])):
            command = ['python.exe', str(target/script), '--nsc', args.nsc, '--clang-root', args.clang_root, *extra]
            result = subprocess.run(command, cwd=target, env=os.environ.copy(), stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT, text=True, errors='replace')
            log = root / (typ+'-'+language+'-'+mode+'.log')
            log.write_text('COMMAND: '+repr(command)+'\nEXIT: '+str(result.returncode)+'\n'+result.stdout, encoding='utf-8')
            if result.returncode or 'OK:' not in result.stdout:
                raise SystemExit(typ+'-'+language+'-'+mode+': fallo; evidencia: '+str(log))
            print(typ+'-'+language+'-'+mode+': correcto', flush=True)
print('OK: registros homogeneos grandes; evidencia:', root, flush=True)
