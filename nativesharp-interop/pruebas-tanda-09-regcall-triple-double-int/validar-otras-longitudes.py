"""Comprueba el retorno entero con dos y cuatro componentes double."""
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
root = Path(tempfile.mkdtemp(prefix='nsc-t09-double-int-longitudes-', dir=r'A:\temp'))
print('Evidencia:', root, flush=True)
for count in (2, 4):
    for language, source in (('c', here), ('cpp', here.parent / 'pruebas-tanda-09-regcall-cpp-triple-double-int')):
        target = root / (str(count) + '-' + language)
        shutil.copytree(source, target, ignore=shutil.ignore_patterns('__pycache__'))
        for file in target.iterdir():
            if not file.is_file():
                continue
            text = file.read_text(encoding='utf-8')
            if file.name == 'api.h':
                text = text.replace('double x, y, z;', 'double x, y;' if count == 2 else 'double x, y, z, w;')
            elif file.name in ('api.c', 'api.cpp'):
                text = text.replace(' + value.z*3', '' if count == 2 else ' + value.z*3 + value.w*4')
            elif file.name in ('Control.c', 'Control.cpp'):
                text = text.replace('{10.25,20.5,3.5}', '{10.25,20.5}' if count == 2 else '{10.25,20.5,3.5,4.25}')
                text = text.replace('==68', '==58' if count == 2 else '==85')
            elif file.suffix == '.ns':
                text = text.replace(' value.z=(double)3.5;', '' if count == 2 else ' value.z=(double)3.5; value.w=(double)4.25;')
                text = text.replace('==68', '==58' if count == 2 else '==85')
            elif file.name == 'validar.py':
                old = r',\s*'.join([r'double\s+[^,)]+'] * 3) + r',\s*i32\s+[^,)]+'
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
print('OK: longitudes adicionales; evidencia:', root, flush=True)
