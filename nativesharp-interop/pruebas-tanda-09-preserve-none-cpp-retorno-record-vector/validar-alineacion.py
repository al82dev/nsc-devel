"""Comprueba almacenamiento C++ alineado a 32 bytes en la matriz preserve_none."""
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
root = Path(tempfile.mkdtemp(prefix='nsc-t09-cpp-vector-align32-', dir='A:/temp'))
fixture = root / 'fixture'
shutil.copytree(source, fixture)
header = fixture / 'api.h'
data = header.read_text(encoding='utf-8')
needle = 'using Float4 = float __attribute__((ext_vector_type(4)));'
if data.count(needle) != 1:
    raise SystemExit('no se encontro el vector de control')
header.write_text(data.replace(needle, 'using Float4 = double __attribute__((ext_vector_type(4)));\nstatic_assert(alignof(Float4) == 32);'), encoding='utf-8')
api = fixture / 'api.cpp'
data = api.read_text(encoding='utf-8')
data = data.replace('p.values[0] += x ==', 'if ((reinterpret_cast<unsigned long long>(&p) & 31) != 0) p.values[0] = -100; p.values[0] += x ==')
data = data.replace('return p->values[0]==n', 'return (reinterpret_cast<unsigned long long>(p) & 31) == 0 && p->values[0]==n')
api.write_text(data, encoding='utf-8')
control = fixture / 'Control.cpp'
data = control.read_text(encoding='utf-8')
control.write_text(data.replace('#include "api.h"', '#include "api.h"\nextern "C" __attribute__((optnone,noinline)) void* memcpy(void* d,const void* s,unsigned long long n) { auto* o=(unsigned char*)d; auto* i=(const unsigned char*)s; for(unsigned long long k=0;k<n;++k) o[k]=i[k]; return d; }'), encoding='utf-8')
for name in ('validar.py', 'validar-precompilado.py'):
    command = [sys.executable, str(fixture / name), '--nsc', args.nsc, '--clang-root', args.clang_root]
    result = subprocess.run(command, cwd=fixture, env=os.environ.copy(), stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
    (root / (name + '.log')).write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
    print(result.stdout, flush=True)
    if result.returncode:
        raise SystemExit(name + ' fallo; evidencia: ' + str(root))
print('OK: alineacion de 32 bytes; evidencia:', root)
