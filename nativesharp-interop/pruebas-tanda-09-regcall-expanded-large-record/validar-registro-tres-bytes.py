"""Contrasta un registro de tres bytes pasado por copia con C y C++."""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile

parser=argparse.ArgumentParser()
parser.add_argument('--nsc',required=True)
parser.add_argument('--clang-root',required=True)
args=parser.parse_args()
fixtures=Path(__file__).resolve().parent.parent
root=Path(tempfile.mkdtemp(prefix='nsc-t09-registro-tres-bytes-',dir=r'A:\temp'))
print('Evidencia:',root,flush=True)
for cpp in (False,True):
    source=fixtures/('pruebas-tanda-09-regcall-'+('cpp-' if cpp else '')+'expanded-large-record')
    dst=root/('cpp' if cpp else 'c'); dst.mkdir()
    for f in source.iterdir():
        if not f.is_file() or f.name in ('validar-rechazos.py',Path(__file__).name): continue
        t=f.read_text(encoding='utf-8')
        if f.suffix in ('.h','.c','.cpp','.ns'):
            t=t.replace('int x; int y; int z; int w; int u; int v;',
                        'unsigned char x; unsigned char y; unsigned char z;')
            for v in ('b','n','q'):
                t=t.replace(v+'.x=1;',v+'.x=(byte)1;') if f.suffix=='.ns' else t
                t=t.replace(v+'.y=2; '+v+'.z=3; '+v+'.w=4; '+v+'.u=5; '+v+'.v=6;',
                            v+'.y=(byte)2; '+v+'.z=(byte)3;') if f.suffix=='.ns' else t
            t=t.replace('Other b={1,2,3,4,5,6};','Other b={1,2,3};')
            t=t.replace('n{1,2,3,4,5,6}','n{1,2,3}').replace('q{1,2,3,4,5,6}','q{1,2,3}')
        if f.suffix=='.py': t=t.replace('expanded-large-','expanded-three-')
        (dst/f.name).write_text(t,encoding='utf-8')
    for script,option in (('validar.py',['--indirecta'] if not cpp else []),
                          ('validar-precompilado.py',['--solo-puntero'])):
        cmd=[sys.executable,str(dst/script),'--nsc',args.nsc,'--clang-root',args.clang_root,*option]
        r=subprocess.run(cmd,cwd=dst,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,errors='replace')
        log=root/(dst.name+'-'+script+'.log')
        log.write_text('COMMAND: '+repr(cmd)+'\nEXIT: '+str(r.returncode)+'\n'+r.stdout,encoding='utf-8')
        if r.returncode or 'OK:' not in r.stdout: raise SystemExit('Fallo: '+str(log))
        print(dst.name,script,'OK; evidencia:',log,flush=True)
print('OK: registro de tres bytes en C y C++; evidencia:',root,flush=True)
