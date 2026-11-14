"""Contrasta un arreglo C++ de vectores con separación física sin relleno."""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
args = parser.parse_args()
source = Path(__file__).resolve().parent
root = Path(tempfile.mkdtemp(prefix='nsc-t09-cpp-vector-array-', dir='A:/temp'))
fixture = root / 'fixture'
shutil.copytree(source, fixture)
header = fixture / 'api.h'
data = header.read_text(encoding='utf-8')
if data.count('Float4 values;') != 1:
    raise SystemExit('no se encontro el campo de control')
data = data.replace('Float4 values;', 'Float4 values[2];')
data += '\nstatic_assert(sizeof(Float4)==16 && sizeof(T09Cpp::Small)==48);\n'
header.write_text(data, encoding='utf-8')
for name in ('Main.ns', 'Producer.ns', 'Control.cpp', 'api.cpp'):
    path = fixture / name
    data = path.read_text(encoding='utf-8').replace('.values[', '.values[1][').replace('->values[', '->values[1][')
    if name == 'api.cpp':
        data = data.replace('p->values[1][0]=0;', 'p->values[0][0]=21; p->values[0][1]=22; p->values[0][2]=23; p->values[0][3]=24; p->values[1][0]=0;')
        data = data.replace('return p->values[1][0]==n', 'return p->values[0][0]==31 && p->values[0][1]==22 && p->values[0][2]==23 && p->values[0][3]==24 && p->values[1][0]==n')
    else:
        data = data.replace('t09_init(&v);', 't09_init(&v); v.values[0][0]=31;')
        data = data.replace('if(v.values[1][0]!=3', 'if(v.values[0][0]!=31 || v.values[0][1]!=22 || v.values[0][2]!=23 || v.values[0][3]!=24 || v.values[1][0]!=3')
    if name == 'Control.cpp':
        data = data.replace('#include "api.h"', '#include "api.h"\nextern "C" __attribute__((optnone,noinline)) void* memcpy(void* d,const void* s,unsigned long long n) { auto* o=(unsigned char*)d; auto* i=(const unsigned char*)s; for(unsigned long long k=0;k<n;++k) o[k]=i[k]; return d; }')
    path.write_text(data, encoding='utf-8')
for name in ('validar.py', 'validar-precompilado.py'):
    command = [sys.executable, str(fixture / name), '--nsc', args.nsc, '--clang-root', args.clang_root]
    result = subprocess.run(command, cwd=fixture, env=os.environ.copy(), stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    (root / (name + '.log')).write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
    print(result.stdout, flush=True)
    if result.returncode:
        raise SystemExit(name + ' fallo; evidencia: ' + str(root))
# A padded vector has a 16-byte stride, not the 12 bytes of its three lanes.
# It must not be projected as a densely packed two-dimensional array.
negative = root / 'vector3-con-relleno'
negative.mkdir()
negative_header = header.read_text(encoding='utf-8').replace('ext_vector_type(4)', 'ext_vector_type(3)')
negative_header += '\nstatic_assert(sizeof(Float4)==16 && 3*sizeof(float)==12);\n'
(negative / 'api.h').write_text(negative_header, encoding='utf-8')
(negative / 'api.cpp').write_text('#include "api.h"\nextern "C" void t09_init(T09Cpp::Small*) {}\n', encoding='utf-8')
(negative / 'Main.ns').write_text('int Main() { T09Cpp.Small v; return (int)v.values[1][0]; }\n', encoding='utf-8')
shutil.copyfile(source / 'VectorCall.nsproj', negative / 'VectorCall.nsproj')
commands = [
    ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-c', str(negative / 'api.cpp'), '-o', str(negative / 'api.obj')],
    ['llvm-lib.exe', '/nologo', '/out:' + str(negative / 'api.lib'), str(negative / 'api.obj')],
    [args.nsc, '--dump-project-oop-hir', str(negative / 'VectorCall.nsproj'), '--library-reference-root', str(negative), '--cpp-clang-root=' + args.clang_root],
]
for i, command in enumerate(commands):
    result = subprocess.run(command, cwd=negative, env=os.environ.copy(), stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    (negative / ('control-%d.log' % i)).write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
    if i < 2 and result.returncode:
        raise SystemExit('fallo preparando el control de stride: ' + str(negative))
    if i == 2 and (result.returncode == 0 or "has no compatible member named 'values'" not in result.stdout):
        raise SystemExit('no se rechazo el stride incompatible: ' + str(negative))
print('OK: arreglo C++ de vectores; evidencia:', root)
