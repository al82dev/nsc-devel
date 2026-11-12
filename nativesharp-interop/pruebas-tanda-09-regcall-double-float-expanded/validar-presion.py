"""Contrasta tres double y cuatro float con presion de registros."""
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
root = Path(tempfile.mkdtemp(prefix='nsc-t09-double-float-presion-', dir=r'A:\temp'))
print('Evidencia:', root, flush=True)
for language, source in (('c', here), ('cpp', here.parent/'pruebas-tanda-09-regcall-cpp-double-float-expanded')):
    target = root / language
    shutil.copytree(source, target, ignore=shutil.ignore_patterns('__pycache__'))
    for file in target.iterdir():
        if not file.is_file():
            continue
        text = file.read_text(encoding='utf-8')
        if file.name == 'api.h':
            text = text.replace('float x, y;', 'float x, y, z, w;')
        elif file.name in ('api.c', 'api.cpp'):
            text = text.replace('extra.x*4 + extra.y*5', 'extra.x*4 + extra.y*5 + extra.z*6 + extra.w*7')
        elif file.name in ('Control.c', 'Control.cpp'):
            text = text.replace('extra={2,3}', 'extra={2,3,4,5}').replace('==91', '==150')
        elif file.suffix == '.ns':
            text = text.replace('extra.y=(float)3;', 'extra.y=(float)3; extra.z=(float)4; extra.w=(float)5;').replace('==91', '==150')
        elif file.name == 'validar.py':
            old = r'float\s+[^,)]+,\s*float\s+[^,)]+'
            new = r'float\s+[^,)]+,\s*float\s+[^,)]+,\s*float\s+[^,)]+,\s*float\s+[^,)]+'
            if old not in text:
                raise SystemExit('No se encontró la verificación LLVM en ' + str(file))
            text = text.replace(old, new)
        file.write_text(text, encoding='utf-8')
    for mode, script, extra in (('directa', 'validar.py', []), ('precompilada-puntero', 'validar-precompilado.py', ['--solo-puntero'])):
        command = ['python.exe', str(target/script), '--nsc', args.nsc, '--clang-root', args.clang_root, *extra]
        result = subprocess.run(command, cwd=target, env=os.environ.copy(), stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, text=True, errors='replace')
        log = root / (language + '-' + mode + '.log')
        log.write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
        if result.returncode or 'OK:' not in result.stdout:
            raise SystemExit(language + '-' + mode + ': fallo; evidencia: ' + str(log))
        print(language + '-' + mode + ': correcto', flush=True)
print('OK: presion de registros; evidencia:', root, flush=True)
