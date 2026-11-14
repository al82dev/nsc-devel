"""Contrasta acceso y ABI de un vector C++ de tres componentes con relleno."""
import argparse
import os
import re
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
    if name in ('Main.ns', 'Producer.ns'):
        data = data.replace('t09_init(&v);', 't09_init(&v); if(sizeof(v.values[0]) != (nuint)%d) return 9;' % (size // 4))
    if args.lane == 'double' and name == 'Control.cpp':
        data = data.replace('#include "api.h"', '#include "api.h"\nextern "C" __attribute__((optnone,noinline)) void* memcpy(void* d,const void* s,unsigned long long n) { auto* o=(unsigned char*)d; auto* i=(const unsigned char*)s; for(unsigned long long k=0;k<n;++k) o[k]=i[k]; return d; }')
    if args.lane == 'double' and name == 'api.cpp':
        if data.count('p.values[0] += x ==') != 1 or data.count('return p->values[0]==n') != 1:
            raise SystemExit('no se encontraron comprobaciones de alineacion')
        data = data.replace('p.values[0] += x ==', 'if ((reinterpret_cast<unsigned long long>(&p) & 31) != 0) p.values[0] = -100; p.values[0] += x ==')
        data = data.replace('return p->values[0]==n', 'return (reinterpret_cast<unsigned long long>(p) & 31) == 0 && p->values[0]==n')
    path.write_text(data, encoding='utf-8')
evidence = {}
for name in ('validar.py', 'validar-precompilado.py'):
    command = [sys.executable, str(fixture / name), '--nsc', args.nsc, '--clang-root', args.clang_root]
    result = subprocess.run(command, cwd=fixture, env=os.environ.copy(), stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    (root / (name + '.log')).write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
    print(result.stdout, flush=True)
    if result.returncode:
        raise SystemExit(name + ' fallo; evidencia: ' + str(root))
    match = re.search(r'^Evidencia:\s*(.+)$', result.stdout, re.MULTILINE)
    if not match:
        raise SystemExit(name + ': falta directorio de evidencia')
    evidence[name] = Path(match.group(1).strip())
# The logical three-lane view is 12/24 bytes; C++ storage is 16/32.
# Do not let layout queries silently report the compact view size.
escape = root / 'layout-no-contiguo'
escape.mkdir()
direct = evidence['validar.py'] / 'estatica-O0'
for filename in ('api.h', 'api.lib', 'VectorCall.nsproj'):
    shutil.copyfile(direct / filename, escape / filename)
for label, statement in (('sizeof', 'var p = sizeof(v.values);'), ('alignof', 'var p = alignof(v.values);')):
    (escape / 'Main.ns').write_text('int Main() { T09Cpp.Small v; ' + statement + ' return 0; }\n', encoding='utf-8')
    command = [args.nsc, '--dump-project-oop-hir', str(escape / 'VectorCall.nsproj'),
               '--library-reference-root', str(escape), '--cpp-clang-root=' + args.clang_root]
    result = subprocess.run(command, cwd=escape, env=os.environ.copy(), stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    (escape / (label + '.log')).write_text('EXIT: ' + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
    if result.returncode == 0 or 'requires its physical type' not in result.stdout:
        raise SystemExit('consulta de layout incorrectamente admitida: ' + str(escape))
pre_root = evidence['validar-precompilado.py'] / 'estatica-O0'
consumer = pre_root / 'escape-layout'
shutil.copytree(pre_root / 'sin-encabezado', consumer)
for label, statement in (('sizeof', 'var p = sizeof(v.values);'), ('alignof', 'var p = alignof(v.values);')):
    (consumer / 'Consumer.ns').write_text('int Main() { T09Cpp.Small v; ' + statement + ' return 0; }\n', encoding='utf-8')
    command = [args.nsc, '--dump-project-oop-hir', str(consumer / 'Consumer.nsproj'), '--cpp-clang-root=' + args.clang_root]
    result = subprocess.run(command, cwd=consumer, env=os.environ.copy(), stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    (root / ('precompilado-' + label + '.log')).write_text('EXIT: ' + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
    if result.returncode == 0 or 'requires its physical type' not in result.stdout:
        raise SystemExit('consulta precompilada incorrectamente admitida: ' + str(root))
print('OK: vector de tres componentes %s con relleno; evidencia:' % args.lane, root)
