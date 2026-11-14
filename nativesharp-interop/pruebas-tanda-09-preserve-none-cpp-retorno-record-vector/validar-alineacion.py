"""Comprueba almacenamiento C++ alineado a 32 o 64 bytes en la matriz preserve_none."""
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
parser.add_argument('--alignment', type=int, choices=(32, 64), default=32)
args = parser.parse_args()
source = Path(__file__).resolve().parent
root = Path(tempfile.mkdtemp(prefix='nsc-t09-cpp-vector-align%d-' % args.alignment, dir='A:/temp'))
fixture = root / 'fixture'
shutil.copytree(source, fixture)
header = fixture / 'api.h'
data = header.read_text(encoding='utf-8')
needle = 'using Float4 = float __attribute__((ext_vector_type(4)));'
if data.count(needle) != 1:
    raise SystemExit('no se encontro el vector de control')
data = data.replace(needle, 'using Float4 = double __attribute__((ext_vector_type(4)));\nstatic_assert(alignof(Float4) == 32);')
if args.alignment == 64:
    if data.count('struct Small {') != 1:
        raise SystemExit('no se encontro el record de control')
    data = data.replace('struct Small {', 'struct alignas(64) Small {')
data += '\nstatic_assert(alignof(T09Cpp::Small) == %d);\n' % args.alignment
header.write_text(data, encoding='utf-8')
api = fixture / 'api.cpp'
data = api.read_text(encoding='utf-8')
if data.count('p.values[0] += x ==') != 1 or data.count('return p->values[0]==n') != 1:
    raise SystemExit('no se encontraron comprobaciones del fixture')
data = data.replace('p.values[0] += x ==', 'if ((reinterpret_cast<unsigned long long>(&p) & %d) != 0) p.values[0] = -100; p.values[0] += x ==' % (args.alignment - 1))
data = data.replace('return p->values[0]==n', 'return (reinterpret_cast<unsigned long long>(p) & %d) == 0 && p->values[0]==n' % (args.alignment - 1))
data += '\nstatic_assert(sizeof(decltype(T09Cpp::Small::values)) == 32 && alignof(decltype(T09Cpp::Small::values)) == 32);\n'
api.write_text(data, encoding='utf-8')
for name in ('Main.ns', 'Producer.ns'):
    path = fixture / name
    text = path.read_text(encoding='utf-8')
    text = text.replace('t09_init(&v);', 't09_init(&v); if(sizeof(v.values) != (nuint)32 || alignof(v.values) != (nuint)32) return 9;')
    path.write_text(text, encoding='utf-8')
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
print('OK: alineacion de %d bytes; evidencia:' % args.alignment, root)
