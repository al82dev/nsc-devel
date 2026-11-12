"""Valida el retorno de un registro flotante con otro registro indirecto."""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
args = parser.parse_args()
fixtures = Path(__file__).resolve().parent.parent
root = Path(tempfile.mkdtemp(prefix='nsc-t09-retorno-record-grande-', dir=r'A:\temp'))
print('Evidencia:', root, flush=True)

for cpp in (False, True):
    source = fixtures/('pruebas-tanda-09-regcall-'+('cpp-' if cpp else '')+'retorno-record-mixto')
    dst = root/('cpp' if cpp else 'c')
    dst.mkdir()
    header_changed = False
    ir_changed = False
    for file in source.iterdir():
        if not file.is_file() or file.name == Path(__file__).name:
            continue
        content = file.read_text(encoding='utf-8')
        if file.suffix in ('.h', '.c', '.cpp', '.ns'):
            before = content
            content = content.replace('float x; int y;', 'int x; int y; int z; int w; int u; int v;')
            if file.name == 'api.h':
                header_changed = before != content
            if file.suffix == '.ns':
                for name in ('b', 'n', 'q'):
                    content = content.replace(name+'.x=(float)1; '+name+'.y=2;',
                        name+'.x=1; '+name+'.y=2; '+name+'.z=3; '+name+'.w=4; '+name+'.u=5; '+name+'.v=6;')
                    content = content.replace(name+'.x==(float)1', name+'.x==1')
            content = content.replace('Other b = {1.0f, 2};', 'Other b = {1, 2, 3, 4, 5, 6};')
            content = content.replace('n{1.0f,2}', 'n{1,2,3,4,5,6}').replace('q{1.0f,2}', 'q{1,2,3,4,5,6}')
        if file.suffix == '.py':
            before = content
            content = content.replace("r'i64\\s+[^,)]+'", "r'ptr\\s+[^,)]+'")
            content = content.replace('return-record-mixed-', 'return-record-large-')
            if file.name == 'validar.py':
                ir_changed = before != content and "r'ptr\\s+[^,)]+'" in content
        (dst/file.name).write_text(content, encoding='utf-8')
    if not header_changed or not ir_changed:
        raise SystemExit('No se pudo preparar la prueba: '+str(dst))
    cases = (('validar.py', ['--indirecta'] if not cpp else [], 'fuente'),
             ('validar-precompilado.py', [], 'precompilado'),
             ('validar-precompilado.py', ['--solo-puntero'], 'precompilado-puntero'))
    for script, options, label in cases:
        cmd = [sys.executable, str(dst/script), '--nsc', args.nsc, '--clang-root', args.clang_root, *options]
        result = subprocess.run(cmd, cwd=dst, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                text=True, errors='replace')
        log = root/(dst.name+'-'+label+'.log')
        log.write_text('COMMAND: '+repr(cmd)+'\nEXIT: '+str(result.returncode)+'\n'+result.stdout, encoding='utf-8')
        if result.returncode or 'OK:' not in result.stdout:
            raise SystemExit('Fallo: '+str(log))
        print(dst.name, label, 'OK; evidencia:', log, flush=True)
print('OK: retorno de registro con argumento indirecto en C y C++; evidencia:', root, flush=True)
