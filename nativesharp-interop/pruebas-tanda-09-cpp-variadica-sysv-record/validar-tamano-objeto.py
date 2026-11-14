"""Contrasta un agregado SysV variadico con un tamano oculto fijo."""
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
parser.add_argument('--dos-punteros', action='store_true')
parser.add_argument('--const-parametros', action='store_true')
parser.add_argument('--modo', type=int, choices=range(4), default=0)
parser.add_argument('--dinamico', action='store_true')
parser.add_argument('--fijos-estrechos', action='store_true')
args = parser.parse_args()
if args.dos_punteros and args.fijos_estrechos:
    parser.error('seleccione dos punteros o parametros estrechos')
source = Path(__file__).resolve().parent
root = Path(tempfile.mkdtemp(prefix='nsc-t09-variadic-record-size-', dir='A:/temp'))
fixture = root / 'fixture'
shutil.copytree(source, fixture)

def replace_once(name, before, after):
    path = fixture / name
    text = path.read_text(encoding='utf-8')
    if text.count(before) != 1:
        raise SystemExit('no se encontro el control esperado en ' + name)
    path.write_text(text.replace(before, after), encoding='utf-8')

pointer = 'int* const' if args.const_parametros else 'int*'
attribute = 'pass_dynamic_object_size' if args.dinamico else 'pass_object_size'
first_attribute = '__attribute__((' + attribute + '(' + str(args.modo) + ')))'
second = (',' + pointer + ' __attribute__((pass_object_size(2))) other' if args.dos_punteros else
          ',signed char marker,bool flag' if args.fijos_estrechos else '')
replace_once('api.h', 'int __attribute__((sysv_abi)) sum(int count,...)',
             'int __attribute__((sysv_abi)) sum(' + pointer + ' ' + first_attribute + ' count' + second + ',...)')
replace_once('api.cpp', 'int sum(int count,...)',
             'int sum(int* const ' + first_attribute + ' count' +
             (',int* const __attribute__((pass_object_size(2))) other' if args.dos_punteros else
              ',signed char marker,bool flag' if args.fijos_estrechos else '') + ',...)')
replace_once('api.cpp', 'cmpl $1, %edi',
             'cmpl $1, (%rdi)\\n\\t"\n            "jne 1f\\n\\t"\n            "cmpq $4, %rsi')
replace_once('api.cpp', 'cmpl $7, %esi', 'cmpl $7, %r8d' if args.dos_punteros or args.fijos_estrechos else 'cmpl $7, %edx')
if args.dos_punteros:
    replace_once('api.cpp', 'cmpq $4, %rsi',
                 'cmpq $4, %rsi\\n\\t"\n            "jne 1f\\n\\t"\n            "cmpl $1, (%rdx)\\n\\t"\n            "jne 1f\\n\\t"\n            "cmpq $4, %rcx')
if args.fijos_estrechos:
    replace_once('api.cpp', 'cmpq $4, %rsi',
                 'cmpq $4, %rsi\\n\\t"\n            "jne 1f\\n\\t"\n            "cmpb $-4, %dl\\n\\t"\n            "jne 1f\\n\\t"\n            "cmpb $1, %cl')
for name in ('Main.ns', 'Producer.ns'):
    replace_once(name, 'return T09Cpp.sum(1,v)', 'int count=1; return T09Cpp.sum(&count,' +
                 ('&count,' if args.dos_punteros else '(sbyte)-4,true,' if args.fijos_estrechos else '') + 'v)')
replace_once('Control.cpp', 'return T09Cpp::sum(1,v)', 'int count=1; return T09Cpp::sum(&count,' +
             ('&count,' if args.dos_punteros else '-4,true,' if args.fijos_estrechos else '') + 'v)')

def run(label, command):
    result = subprocess.run(command, cwd=fixture, env=os.environ.copy(), stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    (root / (label + '.log')).write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode)
                                        + '\n' + result.stdout, encoding='utf-8')
    if result.returncode:
        print(result.stdout, flush=True)
        raise SystemExit(label + ' fallo; evidencia: ' + str(root))
    return result.stdout

run('oracle-obj', ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20',
                   '-c', 'api.cpp', '-o', str(root / 'oracle.obj')])
symbols = run('oracle-simbolos', ['llvm-nm.exe', '--defined-only', str(root / 'oracle.obj')])
matches = [line.split()[-1] for line in symbols.splitlines()
           if '?sum@T09Cpp@@' in line and '__' + attribute + str(args.modo) in line]
if len(matches) != 1:
    raise SystemExit('simbolo de Clang inesperado; evidencia: ' + str(root))
for name in ('validar.py', 'validar-precompilado.py'):
    path = fixture / name
    text = path.read_text(encoding='utf-8')
    if '?sum@T09Cpp@@YAHHZZ' not in text:
        raise SystemExit('no se encontro el simbolo de control: ' + name)
    text = text.replace('?sum@T09Cpp@@YAHHZZ', matches[0])
    text = text.replace(r'\(i32,\s*\.\.\.\)', r'\(ptr,\s*i64,\s*ptr,\s*i64,\s*\.\.\.\)' if args.dos_punteros else
                        r'\(ptr,\s*i64,\s*i8,\s*i1,\s*\.\.\.\)' if args.fijos_estrechos else r'\(ptr,\s*i64,\s*\.\.\.\)')
    path.write_text(text, encoding='utf-8')
for name in ('validar.py', 'validar-precompilado.py'):
    output = run(name, [sys.executable, str(fixture / name), '--nsc', args.nsc, '--clang-root', args.clang_root])
    print(output, flush=True)
    if name == 'validar.py' and args.fijos_estrechos:
        match = re.search(r'Evidencia:\s*(.+)', output)
        if not match:
            raise SystemExit('falta evidencia de la prueba directa')
        direct = Path(match.group(1).strip())
        for mode in ('estatica', 'dll'):
            for opt in range(4):
                ir = (direct / (mode + '-O' + str(opt) + '-ns-ir.log')).read_text(encoding='utf-8')
                relevant = [line for line in ir.splitlines() if matches[0] in line]
                if len(relevant) != 2 or not all('i8 signext' in line and 'i1 zeroext' in line for line in relevant):
                    raise SystemExit('atributos fijos incorrectos; evidencia: ' + str(direct))
print('OK: SysV variadica con tamano oculto y agregado; evidencia:', root)
