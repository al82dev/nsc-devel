"""Contrasta el limite actual de Clang 23.1.0 para preserve_all C++ en Windows."""
import os
from pathlib import Path
import subprocess
import tempfile

fixture = Path(__file__).resolve().parent
root = Path(tempfile.mkdtemp(prefix='nsc-t09-sysv-cpp-scalar-limite-', dir=r'A:\temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
version = subprocess.run(['clang.exe', '--version'], env=env, stdout=subprocess.PIPE,
                         stderr=subprocess.STDOUT, text=True, errors='replace')
if version.returncode or 'clang version 23.1.0' not in version.stdout:
    raise SystemExit('Se requiere Clang 23.1.0')
command = ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-O0',
           '-S', '-emit-llvm', str(fixture / 'Unsupported.cpp'), '-o', str(root / 'output.ll')]
result = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                        text=True, errors='replace')
(root / 'clang.log').write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) +
                                '\n' + result.stdout, encoding='utf-8')
if result.returncode == 0 or 'cannot mangle this calling convention yet' not in result.stdout:
    raise SystemExit('Clang no produjo el limite esperado; evidencia: ' + str(root))
print('OK: limite de Clang reproducido; evidencia:', root)
