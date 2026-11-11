"""T09-003: referencia Clang del ABI SysV para agregados; no valida NSC."""
import os
from pathlib import Path
import re
import subprocess
import tempfile

fixture = Path(__file__).resolve().parent
repo = fixture.parents[1]
root = Path(tempfile.mkdtemp(prefix='nsc-t09-sysv-records-', dir=r'A:\temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)


def run(label, command):
    result = subprocess.run(command, cwd=repo, env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    (root/(label+'.log')).write_text('COMMAND: '+repr(command)+'\nEXIT: '+str(result.returncode)+
                                     '\n'+result.stdout, encoding='utf-8')
    if result.returncode:
        raise SystemExit(label + ': fallo; evidencia: ' + str(root))
    return result.stdout


if 'clang version 23.1.0' not in run('version', ['clang.exe', '--version']):
    raise SystemExit('Se requiere Clang 23.1.0')
output = root/'records.ll'
run('ir', ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c11', '-O0', '-S',
           '-emit-llvm', str(fixture/'abi-records.c'), '-o', str(output)])
ir = output.read_text(encoding='utf-8')
patterns = (r'define[^\n]*x86_64_sysvcc i24 @pass_R3\(i24 ',
            r'define[^\n]*x86_64_sysvcc i32 @pass_R4\(i32 ',
            r'define[^\n]*x86_64_sysvcc i64 @pass_R8\(i64 ',
            r'define[^\n]*x86_64_sysvcc \{ i64, i64 \} @pass_R16\(i64 [^,]+, i64 ',
            r'define[^\n]*x86_64_sysvcc void @pass_R24\([^\n]*sret\([^\n]*byval\(',
            r'define[^\n]*x86_64_sysvcc \{ double, i32 \} @pass_RF\(double [^,]+, i32 ')
for pattern in patterns:
    if not re.search(pattern, ir):
        raise SystemExit('No coincide: '+pattern+'; evidencia: '+str(root))
print('OK: clasificaciones de referencia Clang; evidencia:', root)
