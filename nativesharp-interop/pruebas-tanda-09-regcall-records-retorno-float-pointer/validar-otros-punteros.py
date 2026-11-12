"""Contrasta retornos short* y double* C/C++ en muestras aisladas."""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
args = parser.parse_args()
fixtures = Path(__file__).resolve().parent.parent
root = Path(tempfile.mkdtemp(prefix='nsc-t09-ret-punteros-escalares-', dir=r'A:\temp'))
print('Evidencia:', root, flush=True)

for kind in ('records', 'cpp-records'):
    source = fixtures/('pruebas-tanda-09-regcall-'+kind+'-retorno-float-pointer')
    for target in ('double', 'short'):
        dst = root/(kind+'-'+target)
        dst.mkdir()
        for f in source.iterdir():
            if not f.is_file() or f.name == Path(__file__).name:
                continue
            t = f.read_text(encoding='utf-8')
            if f.suffix in ('.h', '.c', '.cpp', '.ns'):
                t = t.replace('float*', target+'*')
                t = t.replace('float c=0,d=0,e=0,f=0;', target+' c=0,d=0,e=0,f=0;')
                t = t.replace('float c=0,d=0;', target+' c=0,d=0;')
                for v in ('c', 'd', 'e', 'f'):
                    t = t.replace('float '+v+'=(float)0;', target+' '+v+'=('+target+')0;')
            if f.suffix == '.py':
                t = t.replace('ret-float-pointer-', 'ret-'+target+'-pointer-')
            (dst/f.name).write_text(t, encoding='utf-8')
        for script, option in (('validar.py', ['--indirecta'] if kind == 'records' else []),
                               ('validar-precompilado.py', ['--solo-puntero'])):
            command = [sys.executable, str(dst/script), '--nsc', args.nsc, '--clang-root', args.clang_root, *option]
            result = subprocess.run(command, cwd=dst, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                    text=True, errors='replace')
            log = root/(kind+'-'+target+'-'+script+'.log')
            log.write_text('COMMAND: '+repr(command)+'\nEXIT: '+str(result.returncode)+'\n'+result.stdout, encoding='utf-8')
            if result.returncode or 'OK:' not in result.stdout:
                raise SystemExit('Fallo: '+str(log))
            print(kind, target, script, 'OK; evidencia:', log, flush=True)
print('OK: short* y double* en C y C++; evidencia:', root, flush=True)
