"""Comprueba retornos estrechos con signo y sin signo, C y C++."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
args = parser.parse_args()
here = Path(__file__).resolve().parent
root = Path(tempfile.mkdtemp(prefix='nsc-t09-mixto-estrechos-', dir=r'A:\temp'))
print('Evidencia:', root, flush=True)
variants = (('sbyte', 'signed char', 'sbyte', '- 63', '-5', 'i8'),
            ('byte', 'unsigned char', 'byte', '+ 192', '250', 'i8'),
            ('ushort', 'unsigned short', 'ushort', '+ 65472', '65530', 'i16'))
for label, ctype, nstype, expression, expected, carrier in variants:
    for language, source in (('c', here), ('cpp', here.parent / 'pruebas-tanda-09-regcall-cpp-record-mixto-short')):
        target = root / (label + '-' + language)
        shutil.copytree(source, target, ignore=shutil.ignore_patterns('__pycache__'))
        for file in target.iterdir():
            if not file.is_file():
                continue
            text = file.read_text(encoding='utf-8')
            if file.suffix in ('.h', '.c', '.cpp'):
                text = text.replace('short', ctype).replace('- 1292', expression).replace('-1234', expected)
            elif file.suffix == '.ns':
                text = text.replace('short direct=', nstype + ' direct=').replace('short indirect=', nstype + ' indirect=')
                text = text.replace('(short)-1234', '(' + nstype + ')' + expected)
            elif file.name == 'validar.py':
                text = text.replace('x86_regcallcc\\s+i16', 'x86_regcallcc\\s+' + carrier)
            file.write_text(text, encoding='utf-8')
        for mode, script, extra in (('directa', 'validar.py', []), ('precompilada-puntero', 'validar-precompilado.py', ['--solo-puntero'])):
            command = ['python.exe', str(target / script), '--nsc', args.nsc, '--clang-root', args.clang_root, *extra]
            result = subprocess.run(command, cwd=target, env=os.environ.copy(), stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT, text=True, errors='replace')
            log = root / (label + '-' + language + '-' + mode + '.log')
            log.write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
            if result.returncode or 'OK:' not in result.stdout:
                raise SystemExit(label + '-' + language + '-' + mode + ': fallo; evidencia: ' + str(log))
            print(label + '-' + language + '-' + mode + ': correcto', flush=True)
print('OK: variantes estrechas; evidencia:', root, flush=True)
