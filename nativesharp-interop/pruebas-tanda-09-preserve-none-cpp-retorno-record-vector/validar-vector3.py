"""Contrasta acceso y ABI de un vector C++ de tres componentes con relleno."""
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
parser.add_argument('--lane', choices=('float', 'double'), default='float')
args = parser.parse_args()
source = Path(__file__).resolve().parent
root = Path(tempfile.mkdtemp(prefix='nsc-t09-cpp-vector3-', dir='A:/temp'))
fixture = root / 'fixture'
shutil.copytree(source, fixture)
header = fixture / 'api.h'
data = header.read_text(encoding='utf-8')
if data.count('ext_vector_type(4)') != 1:
    raise SystemExit('no se encontro el vector de control')
data = data.replace('ext_vector_type(4)', 'ext_vector_type(3)')
if args.lane == 'double':
    if data.count('using Float4 = float ') != 1:
        raise SystemExit('no se encontro el tipo de componente')
    data = data.replace('using Float4 = float ', 'using Float4 = double ')
size = 16 if args.lane == 'float' else 32
data += '\nstatic_assert(sizeof(Float4)==%d && alignof(Float4)==%d);\n' % (size, size)
header.write_text(data, encoding='utf-8')
for name in ('Main.ns', 'Producer.ns', 'Control.cpp', 'api.cpp'):
    path = fixture / name
    data = path.read_text(encoding='utf-8')
    if name == 'api.cpp':
        replacements = [('p->values[3]=0; ', ''), (' && p->values[3]==6', '')]
    else:
        replacements = [('v.values[3]=6; ', ''), (' || v.values[3]!=6', '')]
    for old, new in replacements:
        if data.count(old) != 1:
            raise SystemExit(name + ': falta la expresion de control ' + old)
        data = data.replace(old, new)
    if args.lane == 'double' and name == 'Control.cpp':
        data = data.replace('#include "api.h"', '#include "api.h"\nextern "C" __attribute__((optnone,noinline)) void* memcpy(void* d,const void* s,unsigned long long n) { auto* o=(unsigned char*)d; auto* i=(const unsigned char*)s; for(unsigned long long k=0;k<n;++k) o[k]=i[k]; return d; }')
    if args.lane == 'double' and name == 'api.cpp':
        if data.count('p.values[0] += x ==') != 1 or data.count('return p->values[0]==n') != 1:
            raise SystemExit('no se encontraron comprobaciones de alineacion')
        data = data.replace('p.values[0] += x ==', 'if ((reinterpret_cast<unsigned long long>(&p) & 31) != 0) p.values[0] = -100; p.values[0] += x ==')
        data = data.replace('return p->values[0]==n', 'return (reinterpret_cast<unsigned long long>(p) & 31) == 0 && p->values[0]==n')
    path.write_text(data, encoding='utf-8')
for name in ('validar.py', 'validar-precompilado.py'):
    command = [sys.executable, str(fixture / name), '--nsc', args.nsc, '--clang-root', args.clang_root]
    result = subprocess.run(command, cwd=fixture, env=os.environ.copy(), stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    (root / (name + '.log')).write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
    print(result.stdout, flush=True)
    if result.returncode:
        raise SystemExit(name + ' fallo; evidencia: ' + str(root))
print('OK: vector de tres componentes %s con relleno; evidencia:' % args.lane, root)
