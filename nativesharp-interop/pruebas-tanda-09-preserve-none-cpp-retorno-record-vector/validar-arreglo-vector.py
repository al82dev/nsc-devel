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
parser.add_argument('--lanes', type=int, choices=(3, 4), default=4)
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
if args.lanes == 3:
    data = data.replace('ext_vector_type(4)', 'ext_vector_type(3)')
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
    if args.lanes == 3 and name in ('Main.ns', 'Producer.ns'):
        data = data.replace('t09_init(&v);', 't09_init(&v); int idx=1;')
        data = data.replace('v.values[1][', 'v.values[idx][')
        data = data.replace('int idx=1;', 'int idx=1; if(sizeof(v.values[idx][0]) != (nuint)4) return 9;')
    if args.lanes == 3:
        for old in ('v.values[1][3]=6; ', 'v.values[idx][3]=6; ', ' || v.values[0][3]!=24', ' || v.values[1][3]!=6', ' || v.values[idx][3]!=6',
                    'p->values[0][3]=24; ', 'p->values[1][3]=0; ',
                    ' && p->values[0][3]==24', ' && p->values[1][3]==6'):
            data = data.replace(old, '')
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
if args.lanes == 3:
    direct_ir = (evidence['validar.py'] / 'estatica-O0-ns-ir.log').read_text(encoding='utf-8')
    if not re.search(r'mul i64 [^,\n]+, 16\b', direct_ir):
        raise SystemExit('falta el paso fisico dinamico de 16 bytes en IR directo: ' + str(root))
    pre_root = evidence['validar-precompilado.py']
    consumer = pre_root / 'estatica-O0' / 'sin-encabezado'
    project = consumer / 'Consumer.nsproj'
    ir_command = [args.nsc, '--dump-project-oop-llvm', str(project), '--cpp-clang-root=' + args.clang_root]
    ir = subprocess.run(ir_command, cwd=consumer, env=os.environ.copy(), stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT, text=True, errors='replace')
    (root / 'precompilado-ir.log').write_text('EXIT: ' + str(ir.returncode) + '\n' + ir.stdout, encoding='utf-8')
    if ir.returncode or not re.search(r'mul i64 [^,\n]+, 16\b', ir.stdout):
        raise SystemExit('falta el paso fisico dinamico en IR precompilado: ' + str(root))
    escape = root / 'escape'
    escape.mkdir()
    direct = evidence['validar.py'] / 'estatica-O0'
    for filename in ('api.h', 'api.lib', 'VectorCall.nsproj'):
        shutil.copyfile(direct / filename, escape / filename)
    cases = {'field': ('var p = &v.values;', 'addressable storage'),
             'row': ('var p = &v.values[0];', 'addressable storage'),
             'copy': ('float[3] row = v.values[0];', 'must be indexed in place'),
             'sizeof-field': ('var p = sizeof(v.values);', 'requires its physical type'),
             'sizeof-row': ('var p = sizeof(v.values[0]);', 'requires its physical type'),
             'alignof-field': ('var p = alignof(v.values);', 'requires its physical type'),
             'alignof-row': ('var p = alignof(v.values[0]);', 'requires its physical type')}
    for label, (statement, diagnostic) in cases.items():
        (escape / 'Main.ns').write_text('int Main() { T09Cpp.Small v; t09_init(&v); ' + statement + ' return 0; }\n', encoding='utf-8')
        command = [args.nsc, '--dump-project-oop-llvm', str(escape / 'VectorCall.nsproj'),
                   '--library-reference-root', str(escape), '--cpp-clang-root=' + args.clang_root]
        result = subprocess.run(command, cwd=escape, env=os.environ.copy(), stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, text=True, errors='replace')
        (escape / (label + '.log')).write_text('EXIT: ' + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
        if result.returncode == 0 or diagnostic not in result.stdout:
            raise SystemExit('no se rechazo escape ' + label + ': ' + str(escape))
    escaped_consumer = pre_root / 'estatica-O0' / 'escape'
    shutil.copytree(consumer, escaped_consumer)
    command = [args.nsc, '--dump-project-oop-llvm', str(escaped_consumer / 'Consumer.nsproj'),
               '--cpp-clang-root=' + args.clang_root]
    for label, statement, diagnostic in (
        ('copy', 'float[3] row = v.values[0];', 'must be indexed in place'),
        ('sizeof', 'var p = sizeof(v.values);', 'requires its physical type'),
        ('alignof', 'var p = alignof(v.values[0]);', 'requires its physical type')):
        (escaped_consumer / 'Consumer.ns').write_text(
            'int Main() { T09Cpp.Small v; ' + statement + ' return 0; }\n', encoding='utf-8')
        result = subprocess.run(command, cwd=escaped_consumer, env=os.environ.copy(), stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, text=True, errors='replace')
        (root / ('escape-precompilado-' + label + '.log')).write_text('EXIT: ' + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
        if result.returncode == 0 or diagnostic not in result.stdout:
            raise SystemExit('no se rechazo ' + label + ' precompilado: ' + str(root))
# Half-precision lanes are not projected yet; keep a negative array control.
negative = root / 'vector-half-no-admitido'
negative.mkdir()
negative_header = header.read_text(encoding='utf-8').replace('using Float4 = float ', 'using Float4 = _Float16 ')
negative_header = negative_header.replace('static_assert(sizeof(Float4)==16 && sizeof(T09Cpp::Small)==48);', '')
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
        raise SystemExit('fallo preparando el control negativo: ' + str(negative))
    if i == 2 and (result.returncode == 0 or "has no compatible member named 'values'" not in result.stdout):
        raise SystemExit('no se rechazo el tipo de vector no admitido: ' + str(negative))
print('OK: arreglo C++ de vectores de %d componentes; evidencia:' % args.lanes, root)
