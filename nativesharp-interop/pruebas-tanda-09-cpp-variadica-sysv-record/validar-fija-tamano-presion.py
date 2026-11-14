"""Comprueba el agregado en pila cuando el tamano oculto agota registros SysV."""
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
root = Path(tempfile.mkdtemp(prefix='nsc-t09-variadic-size-pressure-', dir='A:/temp'))
fixture = root / 'fixture'
shutil.copytree(source, fixture)

def replace_once(name, before, after):
    path = fixture / name
    text = path.read_text(encoding='utf-8')
    if text.count(before) != 1:
        raise SystemExit('no se encontro el control esperado en ' + name)
    path.write_text(text.replace(before, after), encoding='utf-8')

replace_once('api.h', 'struct T09VarValue { double x; int y; };', 'struct T09VarValue { long long x; long long y; };')
replace_once('api.h', 'int __attribute__((sysv_abi)) sum(int count,...)',
             'int __attribute__((sysv_abi)) sum(int a,int b,int c,int d,int* __attribute__((pass_object_size(0))) count,T09VarValue fixed)')
api = fixture / 'api.cpp'
text = api.read_text(encoding='utf-8')
start = text.index('__attribute__((sysv_abi, naked)) int sum(int count,...) {')
end = text.index('\n}\n}', start) + 2
body = r'''__attribute__((sysv_abi, naked)) int sum(int a,int b,int c,int d,int* const __attribute__((pass_object_size(0))) count,T09VarValue fixed) {
    __asm__("cmpl $10, %edi\n\t"
            "jne 1f\n\t"
            "cmpl $20, %esi\n\t"
            "jne 1f\n\t"
            "cmpl $30, %edx\n\t"
            "jne 1f\n\t"
            "cmpl $40, %ecx\n\t"
            "jne 1f\n\t"
            "cmpl $1, (%r8)\n\t"
            "jne 1f\n\t"
            "cmpq $4, %r9\n\t"
            "jne 1f\n\t"
            "cmpl $7, 8(%rsp)\n\t"
            "jne 1f\n\t"
            "cmpl $8, 16(%rsp)\n\t"
            "jne 1f\n\t"
            "movl $32, %eax\n\t"
            "retq\n\t"
            "1: xorl %eax, %eax\n\t"
            "retq\n\t");
}'''
api.write_text(text[:start] + body + text[end:], encoding='utf-8')
(fixture / 'Main.ns').write_text('int Main() { T09VarValue v; v.x=7; v.y=8; int n=1; '
    'return T09Cpp.sum(10,20,30,40,&n,v)==32 && v.x==7 && v.y==8 ? 0 : 1; }\n', encoding='utf-8')
(fixture / 'Producer.ns').write_text('namespace T09 { public class RegBridge { public static int Run<T>(T unused) { '
    'T09VarValue v; v.x=7; v.y=8; int n=1; return T09Cpp.sum(10,20,30,40,&n,v)==32 && '
    'v.x==7 && v.y==8 ? 0 : 1; } } }\n', encoding='utf-8')
(fixture / 'Control.cpp').write_text('#include "api.h"\nint main() { T09VarValue v={7,8}; int n=1; '
    'return T09Cpp::sum(10,20,30,40,&n,v)==32 && v.x==7 && v.y==8 ? 0 : 1; }\n', encoding='utf-8')

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
           if '?sum@T09Cpp@@' in line and '__pass_object_size0' in line]
if len(matches) != 1:
    raise SystemExit('simbolo de Clang inesperado; evidencia: ' + str(root))
for name in ('validar.py', 'validar-precompilado.py'):
    path = fixture / name
    text = path.read_text(encoding='utf-8')
    if '?sum@T09Cpp@@YAHHZZ' not in text:
        raise SystemExit('no se encontro el simbolo de control: ' + name)
    text = text.replace('?sum@T09Cpp@@YAHHZZ', matches[0])
    text = text.replace(r'i32\s+\(i32,\s*\.\.\.\)', r'i32\s+')
    path.write_text(text, encoding='utf-8')
for name in ('validar.py', 'validar-precompilado.py'):
    output = run(name, [sys.executable, str(fixture / name), '--nsc', args.nsc, '--clang-root', args.clang_root])
    print(output, flush=True)
    if name == 'validar.py':
        match = re.search(r'Evidencia:\s*(.+)', output)
        if not match:
            raise SystemExit('falta la evidencia del control directo')
        direct = Path(match.group(1).strip())
        for mode in ('estatica', 'dll'):
            for opt in range(4):
                ir = (direct / (mode + '-O' + str(opt) + '-ns-ir.log')).read_text(encoding='utf-8')
                calls = [line for line in ir.splitlines() if matches[0] in line]
                if len(calls) != 2 or not all('byval([16 x i8]) align 8' in line and '...' not in line for line in calls):
                    raise SystemExit('el agregado no se clasifico por pila; evidencia: ' + str(direct))
print('OK: agregado en pila por presion de registros SysV; evidencia:', root)
