"""Observa la inicialización de va_list; no acredita soporte de preserve_none."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(tempfile.mkdtemp(prefix='nsc-t09-preserve-va-list-', dir='A:/temp'))
shutil.copyfile(Path(__file__).with_name('observar-va-list.cpp'), root / 'probe.cpp')
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)


def run(label, command):
    result = subprocess.run(command, cwd=root, env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    (root / (label + '.log')).write_text('COMMAND: ' + repr(command) + '\nEXIT: '
        + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
    if result.returncode:
        raise SystemExit(label + ': observación distinta o fallo; evidencia: ' + str(root))
    return result.stdout


if 'clang version 23.1.0' not in run('version', ['clang.exe', '--version']):
    raise SystemExit('Se requiere Clang 23.1.0')
for opt in range(4):
    prefix = 'O' + str(opt)
    run(prefix + '-obj', ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20',
        '-' + prefix, '-c', 'probe.cpp', '-o', prefix + '.obj'])
    run(prefix + '-link', ['lld-link.exe', '/nologo', '/entry:main', '/subsystem:console',
        '/nodefaultlib', '/out:' + prefix + '.exe', prefix + '.obj'])
    run(prefix + '-disassembly', ['llvm-objdump.exe', '-d', prefix + '.obj'])
    run(prefix + '-observation', [str(root / (prefix + '.exe'))])
    print(prefix + ': patrón y guardas observados; cdecl sin alteración', flush=True)
print('Observación reproducida; no acredita interoperabilidad. Evidencia:', root)
