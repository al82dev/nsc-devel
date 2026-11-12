"""Contrasta retornos de 8/16 bits C y C++ con muestras generadas aisladas."""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
parser.add_argument('--solo', choices=('i8', 'u8', 'i16', 'u16'))
args = parser.parse_args()
root = Path(tempfile.mkdtemp(prefix='nsc-t09-retornos-estrechos-', dir=r'A:\temp'))
fixtures = Path(__file__).resolve().parent.parent
specs = [('i8', 'signed char', 'sbyte', '-90', '-256'),
         ('u8', 'unsigned char', 'byte', '166', ''),
         ('i16', 'short', 'short', '-20000', '-20166'),
         ('u16', 'unsigned short', 'ushort', '50166', '+50000')]
print('Evidencia:', root, flush=True)
for kind in ('records', 'cpp-records'):
    source = fixtures/('pruebas-tanda-09-regcall-'+kind+'-retorno-int')
    for code, ctype, nstype, expected, extra in specs:
        if args.solo and args.solo != code:
            continue
        dst = root/(kind+'-'+code)
        dst.mkdir()
        for f in source.iterdir():
            if not f.is_file() or f.name in ('validar-rechazos.py', Path(__file__).name):
                continue
            t = f.read_text(encoding='utf-8')
            if f.suffix in ('.h', '.c', '.cpp'):
                t = t.replace('int (__attribute__((regcall))', ctype+' (__attribute__((regcall))')
                t = t.replace('int __attribute__((regcall))', ctype+' __attribute__((regcall))')
                if extra:
                    t = t.replace('return (int)a.', 'return '+extra+' + (int)a.')
            if f.name in ('Control.c', 'Control.cpp'):
                t = t.replace('int c=combine', ctype+' c=combine').replace('int d=get_combine', ctype+' d=get_combine').replace('==166', '=='+expected)
            if f.suffix == '.ns':
                for var in ('c', 'd', 'e', 'f'):
                    t = t.replace('int '+var+'=', nstype+' '+var+'=')
                t = t.replace('==166', '=='+expected)
            if f.suffix == '.py':
                t = t.replace('ret-int-', 'ret-'+code+'-')
                bits = '8' if code.endswith('8') else '16'
                if kind == 'records':
                    t = t.replace(r'x86_regcallcc\s+i32\s+', r'x86_regcallcc\s+i'+bits+r'\s+')
                else:
                    t = t.replace("result_type = r'i32'", "result_type = r'i"+bits+"'")
            (dst/f.name).write_text(t, encoding='utf-8')
        for script, option in (('validar.py', ['--indirecta'] if kind == 'records' else []),
                               ('validar-precompilado.py', ['--solo-puntero'])):
            cmd = [sys.executable, str(dst/script), '--nsc', args.nsc, '--clang-root', args.clang_root, *option]
            result = subprocess.run(cmd, cwd=dst, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                    text=True, errors='replace')
            log = root/(kind+'-'+code+'-'+script+'.log')
            log.write_text('COMMAND: '+repr(cmd)+'\nEXIT: '+str(result.returncode)+'\n'+result.stdout, encoding='utf-8')
            if result.returncode or 'OK:' not in result.stdout:
                raise SystemExit('Fallo: '+str(log))
            print(kind, code, script, 'OK; evidencia:', log, flush=True)
print('OK: retornos estrechos C y C++; evidencia:', root, flush=True)
