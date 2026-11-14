"""Contrasta acceso y ABI de un vector C++ empaquetado y desalineado."""
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
root = Path(tempfile.mkdtemp(prefix='nsc-t09-cpp-vector-packed-', dir='A:/temp'))
fixture = root / 'fixture'
shutil.copytree(source, fixture)
header = fixture / 'api.h'
data = header.read_text(encoding='utf-8')
if data.count('struct Small {') != 1 or data.count('double wide;') != 1:
    raise SystemExit('no se encontro el record de control')
data = data.replace('struct Small {', 'struct __attribute__((packed, aligned(8))) Small { char pad;')
data = data.replace('double wide;', 'double wide __attribute__((aligned(8)));')
data += '\nstatic_assert(__builtin_offsetof(T09Cpp::Small, values)==1 && alignof(T09Cpp::Small)==8);\n'
header.write_text(data, encoding='utf-8')
api = fixture / 'api.cpp'
data = api.read_text(encoding='utf-8')
if data.count('p->values[0]=0;') != 1 or data.count('return p->values[0]==n') != 1:
    raise SystemExit('no se encontraron operaciones de control')
data = data.replace('p->values[0]=0;', 'p->pad=37; p->values[0]=0;')
data = data.replace('return p->values[0]==n', 'return p->pad==37 && p->values[0]==n')
api.write_text(data, encoding='utf-8')
def check_ir(ir, label):
    loads = re.findall(r'\bload float, ptr %raw\.element\.addr[^\n]*', ir)
    stores = re.findall(r'\bstore float [^\n]*ptr %raw\.element\.addr[^\n]*', ir)
    if len(loads) < 4 or len(stores) < 4 or any(not re.search(r'\balign 1(?:\D|$)', line) for line in loads + stores):
        raise SystemExit('alineacion IR incorrecta: ' + str(label))

for name in ('validar.py', 'validar-precompilado.py'):
    command = [sys.executable, str(fixture / name), '--nsc', args.nsc, '--clang-root', args.clang_root]
    result = subprocess.run(command, cwd=fixture, env=os.environ.copy(), stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    (root / (name + '.log')).write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
    print(result.stdout, flush=True)
    if result.returncode:
        raise SystemExit(name + ' fallo; evidencia: ' + str(root))
    if name == 'validar.py':
        match = re.search(r'^Evidencia:\s*(.+)$', result.stdout, re.MULTILINE)
        if not match:
            raise SystemExit('no se encontro evidencia de IR: ' + str(root))
        ir_root = Path(match.group(1).strip())
        for mode in ('estatica', 'dll'):
            for opt in range(4):
                ir = (ir_root / ('%s-O%d-ns-ir.log' % (mode, opt))).read_text(encoding='utf-8')
                check_ir(ir, ir_root)
    else:
        match = re.search(r'^Evidencia:\s*(.+)$', result.stdout, re.MULTILINE)
        if not match:
            raise SystemExit('no se encontro evidencia precompilada: ' + str(root))
        ir_root = Path(match.group(1).strip())
        project = ir_root / 'estatica-O0' / 'sin-encabezado' / 'Consumer.nsproj'
        ir_command = [args.nsc, '--dump-project-oop-llvm', str(project), '--cpp-clang-root=' + args.clang_root]
        ir_result = subprocess.run(ir_command, cwd=fixture, env=os.environ.copy(), stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, text=True, errors='replace')
        (root / 'precompilado-ir.log').write_text('COMMAND: ' + repr(ir_command) + '\nEXIT: ' + str(ir_result.returncode) + '\n' + ir_result.stdout, encoding='utf-8')
        if ir_result.returncode:
            raise SystemExit('fallo el IR precompilado: ' + str(root))
        check_ir(ir_result.stdout, root / 'precompilado-ir.log')
print('OK: vector C++ empaquetado; evidencia:', root)
