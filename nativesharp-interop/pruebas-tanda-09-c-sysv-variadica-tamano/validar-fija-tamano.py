"""Contrasta un agregado C variadico con argumento oculto de tamano."""
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
args = parser.parse_args()
source = Path(__file__).resolve().parent
root = Path(tempfile.mkdtemp(prefix='nsc-t09-c-sysv-var-record-size-', dir='A:/temp'))
fixture = root / 'fixture'
shutil.copytree(source, fixture)

def replace_once(name, before, after):
    path = fixture / name
    text = path.read_text(encoding='utf-8')
    if text.count(before) != 1:
        raise SystemExit('no se encontro el control esperado en ' + name)
    path.write_text(text.replace(before, after), encoding='utf-8')

replace_once('api.h', 'T09_STORAGE __attribute__((sysv_abi)) int t09_csum(',
             'struct T09CVar { double x; int y; };\nT09_STORAGE __attribute__((sysv_abi)) int t09_csum(')
replace_once('api.h', 'int *p __attribute__((pass_object_size(0))), signed char marker, _Bool flag, ...',
             'int *p __attribute__((pass_object_size(0))), struct T09CVar fixed, int last')
replace_once('api.c', 'int *const p __attribute__((pass_object_size(0))), signed char marker, _Bool flag, ...',
             'int *const p __attribute__((pass_object_size(0))), struct T09CVar fixed, int last')
replace_once('api.c', '            "cmpb $-4, %dl\\n\\t"\n            "jne 1f\\n\\t"\n'
                     '            "cmpb $1, %cl\\n\\t"\n            "jne 1f\\n\\t"\n'
                     '            "cmpl $-3, %r8d\\n\\t"\n            "jne 1f\\n\\t"\n', '')
replace_once('api.c', 'cmpl $7, %r9d', 'cmpl $7, %edx')
replace_once('api.c', 'cmpl $7, %edx', 'cmpl $7, %edx\\n\\t"\n            "jne 1f\\n\\t"\n            "cmpl $3, %ecx')
replace_once('api.c', '            "cmpb $1, %al\\n\\t"\n            "jb 1f\\n\\t"\n', '')
replace_once('api.c', '0x4004000000000000', '0x4002000000000000')
replace_once('Control.c', 'return t09_csum(&count, -4, 1, -3, 2.5f, 7)',
             'struct T09CVar v = {2.25, 7}; return t09_csum(&count, v, 3)')
for name in ('Main.ns', 'Producer.ns'):
    replace_once(name, 'return t09_csum(&count, (sbyte)-4, true, (sbyte)-3, 2.5f, (ushort)7)',
                 'T09CVar v; v.x=2.25; v.y=7; return t09_csum(&count, v, 3)')
replace_once('validar.py', "'x86_64_sysvcc' in line and 'i8 signext' in line and 'i1 zeroext' in line",
             "'x86_64_sysvcc' in line")
command = [sys.executable, str(fixture / 'validar.py'), '--nsc', args.nsc, '--clang-root', args.clang_root]
result = subprocess.run(command, cwd=fixture, env=os.environ.copy(), stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT, text=True, errors='replace')
(root / 'validar.log').write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode)
                                 + '\n' + result.stdout, encoding='utf-8')
print(result.stdout, flush=True)
if result.returncode:
    raise SystemExit('fallo; evidencia: ' + str(root))
match = re.search(r'Evidencia:\s*(.+)', result.stdout)
if not match:
    raise SystemExit('falta evidencia del IR')
direct = Path(match.group(1).strip())
for mode in ('estatica', 'dll'):
    for opt in range(4):
        ir = (direct / (mode + '-O' + str(opt) + '-ir.log')).read_text(encoding='utf-8')
        lines = [line for line in ir.splitlines() if '@t09_csum(' in line]
        if len(lines) != 2 or not any(re.search(r'@t09_csum\(ptr, i64, double, i32, i32\)', line) for line in lines) or any('...' in line for line in lines):
            raise SystemExit('firma fisica incorrecta; evidencia: ' + str(direct))
print('OK: agregado fijo C SysV no variadico con tamano oculto; evidencia:', root)
