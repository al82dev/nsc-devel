"""Contrasta records compactos de dos y cuatro bytes junto a uno expandido."""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile

parser=argparse.ArgumentParser()
parser.add_argument('--nsc',required=True)
parser.add_argument('--clang-root',required=True)
parser.add_argument('--solo',choices=('2','4'))
args=parser.parse_args()
fixtures=Path(__file__).resolve().parent.parent
root=Path(tempfile.mkdtemp(prefix='nsc-t09-tamanos-compactos-',dir=r'A:\temp'))
print('Evidencia:',root,flush=True)
for cpp in (False,True):
    src=fixtures/('pruebas-tanda-09-regcall-'+('cpp-' if cpp else '')+'expanded-mixed-record')
    for size in ('2','4'):
        if args.solo and args.solo!=size: continue
        dst=root/(('cpp-' if cpp else 'c-')+size); dst.mkdir()
        for f in src.iterdir():
            if not f.is_file() or f.name in ('validar-rechazos.py',Path(__file__).name): continue
            t=f.read_text(encoding='utf-8')
            if f.suffix in ('.h','.cpp','.c','.ns'):
                field='unsigned char' if size=='2' else 'short'
                nstype='byte' if size=='2' else 'short'
                t=t.replace('float x; int y;',field+' x; '+field+' y;')
                t=t.replace('b.x=(float)1','b.x=('+nstype+')1').replace('b.y=2;','b.y=('+nstype+')2;')
                t=t.replace('n.x=(float)1','n.x=('+nstype+')1').replace('n.y=2;','n.y=('+nstype+')2;')
                t=t.replace('q.x=(float)1','q.x=('+nstype+')1').replace('q.y=2;','q.y=('+nstype+')2;')
                t=t.replace('b.x==(float)1','b.x==('+nstype+')1')
                t=t.replace('Other b={1.0f,2};','Other b={1,2};')
                t=t.replace('n{1.0f,2}','n{1,2}').replace('q{1.0f,2}','q{1,2}')
            if f.suffix=='.py':
                t=t.replace('expanded-mixed-','expanded-compact'+size+'-')
                t=t.replace("r'i64\\s+[^,)]+'","r'i"+('16' if size=='2' else '32')+r"\s+[^,)]+'")
            (dst/f.name).write_text(t,encoding='utf-8')
        for script, option in (('validar.py',['--indirecta'] if not cpp else []),
                               ('validar-precompilado.py',['--solo-puntero'])):
            cmd=[sys.executable,str(dst/script),'--nsc',args.nsc,'--clang-root',args.clang_root,*option]
            r=subprocess.run(cmd,cwd=dst,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,errors='replace')
            log=root/(dst.name+'-'+script+'.log')
            log.write_text('COMMAND: '+repr(cmd)+'\nEXIT: '+str(r.returncode)+'\n'+r.stdout,encoding='utf-8')
            if r.returncode or 'OK:' not in r.stdout: raise SystemExit('Fallo: '+str(log))
            print(dst.name,script,'OK; evidencia:',log,flush=True)
print('OK: tamaños compactos C y C++; evidencia:',root,flush=True)
