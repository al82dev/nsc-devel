"""Contrasta el retorno de 64 bits sin signo con el bit alto activo en C y C++."""
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
root = Path(tempfile.mkdtemp(prefix='nsc-t09-mixto-u64-', dir=r'A:\temp'))
print('Evidencia:', root, flush=True)
for language, source in (('c', here), ('cpp', here.parent / 'pruebas-tanda-09-regcall-cpp-record-mixto-i64')):
    target = root / language
    shutil.copytree(source, target, ignore=shutil.ignore_patterns('__pycache__'))
    for file in target.iterdir():
        if not file.is_file():
            continue
        text = file.read_text(encoding='utf-8')
        if file.suffix in ('.h', '.c', '.cpp'):
            text = text.replace('long long', 'unsigned long long')
            text = text.replace('4294967296LL', '9223372036854775808ULL')
            text = text.replace('4294967354LL', '9223372036854775866ULL')
        elif file.suffix == '.ns':
            text = text.replace('long direct=', 'ulong direct=').replace('long indirect=', 'ulong indirect=')
            text = text.replace('(long)4294967354', '9223372036854775866UL')
        file.write_text(text, encoding='utf-8')
    for label, script, extra in (('directa', 'validar.py', []), ('precompilada-puntero', 'validar-precompilado.py', ['--solo-puntero'])):
        command = ['python.exe', str(target / script), '--nsc', args.nsc, '--clang-root', args.clang_root, *extra]
        result = subprocess.run(command, cwd=target, env=os.environ.copy(), stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, text=True, errors='replace')
        log = root / (language + '-' + label + '.log')
        log.write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
        if result.returncode or 'OK:' not in result.stdout:
            raise SystemExit(language + '-' + label + ': fallo; evidencia: ' + str(log))
        print(language + '-' + label + ': correcto', flush=True)
print('OK: controles sin signo; evidencia:', root, flush=True)
