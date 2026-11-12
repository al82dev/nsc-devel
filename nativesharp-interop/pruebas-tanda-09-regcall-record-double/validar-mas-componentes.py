"""Contrasta registros de tres y cuatro double en C y C++."""
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
root = Path(tempfile.mkdtemp(prefix='nsc-t09-double-mas-componentes-', dir=r'A:\temp'))
print('Evidencia:', root, flush=True)
for count in (3, 4):
    fields = ', z' + (', w' if count == 4 else '')
    initial = ',3.5' + (',4.25' if count == 4 else '')
    ns_initial = ' value.z=(double)3.5;' + (' value.w=(double)4.25;' if count == 4 else '')
    calculation = ' + value.z*3' + (' + value.w*4' if count == 4 else '')
    expected = '85.75' if count == 4 else '68.75'
    for language, source in (('c', here), ('cpp', here.parent / 'pruebas-tanda-09-regcall-cpp-record-double')):
        target = root / (str(count) + '-' + language)
        shutil.copytree(source, target, ignore=shutil.ignore_patterns('__pycache__'))
        for file in target.iterdir():
            if not file.is_file():
                continue
            text = file.read_text(encoding='utf-8')
            if file.name == 'api.h':
                text = text.replace('double x, y;', 'double x, y' + fields + ';')
            elif file.name in ('api.c', 'api.cpp'):
                text = text.replace('value.x + value.y*2 + bias', 'value.x + value.y*2' + calculation + ' + bias')
            elif file.name in ('Control.c', 'Control.cpp'):
                text = text.replace('{10.25,20.5}', '{10.25,20.5' + initial + '}').replace('58.25', expected)
            elif file.suffix == '.ns':
                text = text.replace('value.y=(double)20.5;', 'value.y=(double)20.5;' + ns_initial).replace('58.25', expected)
            elif file.name == 'validar.py':
                old = r'double\s+[^,)]+,\s*double\s+[^,)]+,\s*i32\s+[^,)]+'
                new = r',\s*'.join([r'double\s+[^,)]+'] * count) + r',\s*i32\s+[^,)]+'
                if old not in text:
                    raise SystemExit('No se encontró la verificación LLVM en ' + str(file))
                text = text.replace(old, new)
            file.write_text(text, encoding='utf-8')
        for mode, script, extra in (('directa', 'validar.py', []), ('precompilada-puntero', 'validar-precompilado.py', ['--solo-puntero'])):
            command = ['python.exe', str(target / script), '--nsc', args.nsc, '--clang-root', args.clang_root, *extra]
            result = subprocess.run(command, cwd=target, env=os.environ.copy(), stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT, text=True, errors='replace')
            log = root / (str(count) + '-' + language + '-' + mode + '.log')
            log.write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
            if result.returncode or 'OK:' not in result.stdout:
                raise SystemExit(str(count) + '-' + language + '-' + mode + ': fallo; evidencia: ' + str(log))
            print(str(count) + '-' + language + '-' + mode + ': correcto', flush=True)
print('OK: registros de tres y cuatro double; evidencia:', root, flush=True)
