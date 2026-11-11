"""Comprueba el IR de Clang para records en llamadas SysV variadicas."""
import os
from pathlib import Path
import re
import subprocess
import tempfile

fixture = Path(__file__).resolve().parent
root = Path(tempfile.mkdtemp(prefix='nsc-t09-sysv-varrecord-ir-', dir='A:/temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
def run(label, command):
    p = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                       text=True, errors='replace')
    (root/(label+'.log')).write_text('COMMAND: '+repr(command)+'\nEXIT: '+str(p.returncode)+'\n'+p.stdout, encoding='utf-8')
    if p.returncode: raise SystemExit(label+': fallo; evidencia: '+str(root)+'\n'+p.stdout)
    return p.stdout
if 'clang version 23.1.0' not in run('version', ['clang.exe', '--version']):
    raise SystemExit('Se requiere Clang 23.1.0')
run('ir', ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c11', '-O0',
           '-S', '-emit-llvm', str(fixture/'oraculo.c'), '-o', str(root/'oraculo.ll')])
ir = (root/'oraculo.ll').read_text(encoding='utf-8')
calls = re.findall(r'call[^\n]*x86_64_sysvcc[^\n]*@vf\([^\n]*', ir)
if len(calls) != 3 or not re.search(r'i32 noundef 3, i64 [^,]+, double [^,]+, i32 [^,]+, ptr noundef byval\(%struct.Large\) align 8', calls[0]):
    raise SystemExit('El plan sin presion de registros no coincide; evidencia: '+str(root))
for call in calls[1:]:
    if not re.search(r'ptr noundef byval\(%struct.Mixed\) align 8', call):
        raise SystemExit('El plan con presion de registros no coincide; evidencia: '+str(root))
print('OK: record directo e indirecto con presion de registros; evidencia:', root)
