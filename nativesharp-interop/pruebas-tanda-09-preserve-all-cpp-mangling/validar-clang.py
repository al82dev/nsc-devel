"""Reproduce el límite de mangling de Clang sin acreditar interoperabilidad NSC."""
import os
from pathlib import Path
import subprocess
import tempfile

fixture = Path(__file__).resolve().parent
root = Path(tempfile.mkdtemp(prefix='nsc-t09-preserve-all-cpp-mangling-', dir=r'A:\temp'))
env = dict(os.environ, TEMP=str(root), TMP=str(root))
print('Evidencia:', root, flush=True)

def run(name, command):
    result = subprocess.run(command, cwd=fixture, env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    log = root / (name + '.log')
    log.write_text('COMMAND: '+repr(command)+'\nEXIT: '+str(result.returncode)+'\n'+result.stdout, encoding='utf-8')
    return result

version = run('version', ['clang.exe', '--version'])
if version.returncode or 'clang version 23.1.0' not in version.stdout:
    raise SystemExit('Se requiere Clang 23.1.0')
flags = ['-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-c']
cpp = run('cpp', ['clang.exe', *flags, str(fixture/'api.cpp'), '-o', str(root/'api.obj')])
if cpp.returncode == 0 or 'cannot mangle this calling convention yet' not in cpp.stdout:
    raise SystemExit('Resultado C++ distinto; revisar evidencia: '+str(root/'cpp.log'))
extern_c = run('extern-c', ['clang.exe', *flags, str(fixture/'api-extern-c.cpp'), '-o', str(root/'api-extern-c.obj')])
if extern_c.returncode:
    raise SystemExit('Fallo el control extern C: '+str(root/'extern-c.log'))
print('OK: limite de Clang reproducido; no acredita interoperabilidad NSC; evidencia:', root, flush=True)
