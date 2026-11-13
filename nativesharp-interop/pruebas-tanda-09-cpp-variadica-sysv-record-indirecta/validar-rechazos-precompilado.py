"""Rechaza el consumo sin encabezado cuando faltan pruebas ABI en el catalogo."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
parser.add_argument('--mutador', required=True)
parser.add_argument('--caso', required=True, help='Directorio estatica-O0 de validar-precompilado.py')
args = parser.parse_args()
source = Path(args.caso).resolve()
root = Path(tempfile.mkdtemp(prefix='nsc-t09-cpp-var-record-indirecta-precomp-negativos-', dir=r'A:\temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
for mode in ('syntax', 'alias'):
    case = root / mode
    shutil.copytree(source, case)
    metadata = case / 'producer' / 'Producer.nsmeta'
    mutation = subprocess.run([args.mutador, '--remove-cpp-variadic-replay-evidence', str(metadata), mode],
                              cwd=case, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
    (root / (mode + '-mutacion.log')).write_text('EXIT: ' + str(mutation.returncode) + '\n' + mutation.stdout, encoding='utf-8')
    if mutation.returncode:
        raise SystemExit('No se pudo modificar el catalogo; evidencia: ' + str(root))
    consumer = case / 'sin-encabezado'
    command = [args.nsc, '--emit-project-oop-exe', str(consumer / 'Consumer.nsproj'),
               '--cpp-clang-root=' + args.clang_root, '--no-artifact-cache', '-O0',
               '-o', str(consumer / 'out' / 'Rejected.exe')]
    result = subprocess.run(command, cwd=case, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
    (root / (mode + '-consumidor.log')).write_text(repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
    expected = 'sysv_abi function pointer signature is not yet supported' if mode == 'syntax' else "expected ';', found 'callback'"
    if result.returncode == 0 or expected not in result.stdout:
        raise SystemExit('No se obtuvo el rechazo esperado para ' + mode + '; evidencia: ' + str(root))
    print('OK: rechazado sin', mode, flush=True)
print('Evidencia:', root)