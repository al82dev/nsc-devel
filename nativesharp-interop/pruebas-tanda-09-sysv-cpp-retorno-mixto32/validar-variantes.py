"""Compara retornos C++ SysV de dos piezas con distintas disposiciones."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
args = parser.parse_args()
source = Path(__file__).resolve().parent
root = Path(tempfile.mkdtemp(prefix='nsc-t09-retornos-cpp-variantes-', dir=r'A:\temp'))
variants = {
    'fff': ('float x,y,z;', '10,20,30', 's.x=10; s.y=20; s.z=30;', 's.x==10 && s.y==20 && s.z==30', 'a.x==11 && a.y==20 && a.z==33', r'<2 x float>,\s*float'),
    'ffff': ('float x,y,z,w;', '10,20,30,40', 's.x=10; s.y=20; s.z=30; s.w=40;', 's.x==10 && s.y==20 && s.z==30 && s.w==40', 'a.x==11 && a.y==20 && a.z==33 && a.w==40', r'<2 x float>,\s*<2 x float>'),
    'dd': ('double x,z;', '10,30', 's.x=10; s.z=30;', 's.x==10 && s.z==30', 'a.x==11 && a.z==33', r'double,\s*double'),
    'di': ('double x; int z;', '10,30', 's.x=10; s.z=30;', 's.x==10 && s.z==30', 'a.x==11 && a.z==33', r'double,\s*i32'),
    'id': ('int x; double z;', '10,30', 's.x=10; s.z=30;', 's.x==10 && s.z==30', 'a.x==11 && a.z==33', r'i32,\s*double'),
    'iif': ('int x,y; float z;', '10,20,30', 's.x=10; s.y=20; s.z=30;', 's.x==10 && s.y==20 && s.z==30', 'a.x==11 && a.y==20 && a.z==33', r'i64,\s*float'),
    'ffii': ('float x,y; int z,w;', '10,20,30,40', 's.x=10; s.y=20; s.z=30; s.w=40;', 's.x==10 && s.y==20 && s.z==30 && s.w==40', 'a.x==11 && a.y==20 && a.z==33 && a.w==40', r'<2 x float>,\s*i64'),
}
print('Evidencia:', root, flush=True)
for label, (fields, initial, init_ns, original, result, pair) in variants.items():
    dest = root / label
    shutil.copytree(source, dest)
    header = dest / 'api.h'
    header.write_text(header.read_text().replace('float x,y; int z;', fields))
    for name in ('Main.ns', 'Producer.ns', 'Control.cpp'):
        path = dest / name
        text = path.read_text().replace('s.x=10; s.y=20; s.z=30;', init_ns)
        text = text.replace('s{10,20,30}', 's{' + initial + '}')
        text = text.replace('s.x==10 && s.y==20 && s.z==30', original)
        text = text.replace('a.x==11 && a.y==20 && a.z==33', result)
        text = text.replace('b.x==11 && b.y==20 && b.z==33', result.replace('a.', 'b.'))
        path.write_text(text)
    test = dest / 'validar.py'
    text = test.read_text().replace(r'<2 x float>,\s*i32', pair)
    text = text.replace(r'\(<2 x float>', r'\(' + pair.split(',')[0])
    test.write_text(text)
    for script in ('validar.py', 'validar-precompilado.py'):
        command = [sys.executable, str(dest / script), '--nsc', args.nsc, '--clang-root', args.clang_root]
        run = subprocess.run(command, env=os.environ.copy(), stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT, text=True, errors='replace')
        (root / (label + '-' + script + '.log')).write_text(
            'COMMAND: ' + repr(command) + '\nEXIT: ' + str(run.returncode) + '\n' + run.stdout, encoding='utf-8')
        print(label, script, 'EXIT', run.returncode, flush=True)
        if run.returncode:
            print(run.stdout, flush=True)
            raise SystemExit('Fallo; evidencia: ' + str(root))
print('OK: siete variantes; evidencia:', root)
