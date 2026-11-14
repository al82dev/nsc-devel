"""Contrasta tamaños y alineaciones de vectores fijos con Clang."""
from pathlib import Path
import subprocess
import tempfile
source = Path(__file__).resolve().parent
root = Path(tempfile.mkdtemp(prefix='nsc-t09-vector-layout-', dir='A:/temp'))
command = ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-fsyntax-only', str(source / 'VectorLayout.cpp')]
p = subprocess.run(command, cwd=source, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
(root / 'clang.log').write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(p.returncode) + '\n' + p.stdout, encoding='utf-8')
if p.returncode:
    raise SystemExit('fallo del control Clang; evidencia: ' + str(root))
print('OK: tamaños y alineaciones de Clang; evidencia:', root)
