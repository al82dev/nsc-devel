"""Contrasta long y unsigned long de 32 bits en Win64, en C y C++."""
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
root=Path(tempfile.mkdtemp(prefix='nsc-t09-ret-long-win64-',dir=r'A:\temp'))
print('Evidencia:',root,flush=True)
for kind in ('records','cpp-records'):
    for code, old, new in (('int','int','long'),('u32','unsigned int','unsigned long')):
        src=fixtures/('pruebas-tanda-09-regcall-'+kind+'-retorno-'+code)
        dst=root/(kind+'-'+code); dst.mkdir()
        for f in src.iterdir():
            if not f.is_file() or f.name in ('validar-rechazos.py','validar-retornos-estrechos.py',Path(__file__).name): continue
            t=f.read_text(encoding='utf-8')
            if f.suffix in ('.h','.c','.cpp'):
                t=t.replace(old+' (__attribute__((regcall))',new+' (__attribute__((regcall))')
                t=t.replace(old+' __attribute__((regcall))',new+' __attribute__((regcall))')
                if f.name=='Control.c':
                    t=t.replace(old+' c=combine',new+' c=combine').replace(old+' d=get_combine',new+' d=get_combine')
            (dst/f.name).write_text(t,encoding='utf-8')
        for script, option in (('validar.py',['--indirecta'] if kind=='records' else []),
                               ('validar-precompilado.py',['--solo-puntero'])):
            cmd=[sys.executable,str(dst/script),'--nsc',args.nsc,'--clang-root',args.clang_root,*option]
            result=subprocess.run(cmd,cwd=dst,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,errors='replace')
            log=root/(kind+'-'+code+'-'+script+'.log')
            log.write_text('COMMAND: '+repr(cmd)+'\nEXIT: '+str(result.returncode)+'\n'+result.stdout,encoding='utf-8')
            if result.returncode or 'OK:' not in result.stdout: raise SystemExit('Fallo: '+str(log))
            print(kind,code,script,'OK; evidencia:',log,flush=True)
print('OK: long y unsigned long de Win64; evidencia:',root,flush=True)
