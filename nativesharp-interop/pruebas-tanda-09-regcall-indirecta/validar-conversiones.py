"""Rechaza conversiones de funciones entre regcall y cdecl."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
args = parser.parse_args()
fixture = Path(__file__).resolve().parent
root = Path(tempfile.mkdtemp(prefix='nsc-t09-regcall-conversiones-', dir=r'A:\temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
print('Evidencia:', root, flush=True)
for name in ('api.h', 'api.c', 'VectorCall.nsproj'):
    shutil.copyfile(fixture / name, root / name)
with (root / 'api.h').open('a', encoding='utf-8') as header:
    header.write('\nint __attribute__((regcall)) t09_regcall(int, int, int);\n'
                 'int t09_normal(int, int, int);\n'
                 'typedef int (*T09CdeclCallback)(int, int, int);\n'
                 'typedef struct T09Unsupported { float x,y,z; } T09Unsupported;\n'
                 'typedef int (*T09InnerCallback)(int);\n'
                 'T09InnerCallback t09_get_inner(void);\n'
                 'typedef T09Unsupported (__attribute__((regcall)) *T09UnsupportedCallback)(T09Unsupported,int,T09InnerCallback);\n'
                 'T09UnsupportedCallback t09_get_unsupported_callback(void);\n')
with (root / 'api.c').open('a', encoding='utf-8') as source:
    source.write('\nint t09_normal(int a, int b, int c) { return a+b+c; }\n'
                 'static int t09_inner(int x) { return x; }\n'
                 'T09InnerCallback t09_get_inner(void) { return t09_inner; }\n'
                 'static T09Unsupported __attribute__((regcall)) t09_unsupported(T09Unsupported x, int unused, T09InnerCallback other) { return x; }\n'
                 'T09UnsupportedCallback t09_get_unsupported_callback(void) { return t09_unsupported; }\n')
for label, command in (
    ('compile', ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-c', str(root / 'api.c'), '-o', str(root / 'api.obj')]),
    ('archive', ['llvm-lib.exe', '/nologo', '/out:' + str(root / 'api.lib'), str(root / 'api.obj')]),
):
    result = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
    (root / (label + '.log')).write_text('EXIT: ' + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
    if result.returncode: raise SystemExit(label + ': fallo; evidencia: ' + str(root))
for label, declaration in (('cdecl-to-regcall', 'T09RegCallback callback = t09_normal;'),
                           ('regcall-to-cdecl', 'T09CdeclCallback callback = t09_regcall;')):
    (root / 'Main.ns').write_text('int Main() { ' + declaration + ' return callback(1,2,3); }', encoding='utf-8')
    command = [args.nsc, '--dump-project-oop-llvm', str(root / 'VectorCall.nsproj'),
               '--cpp-clang-root=' + args.clang_root, '--library-reference-root', str(root)]
    result = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
    (root / (label + '.log')).write_text('EXIT: ' + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
    if result.returncode == 0 or 'conversion between regcall and another function pointer convention' not in result.stdout:
        raise SystemExit(label + ': no produjo el rechazo esperado; evidencia: ' + str(root))
(root / 'Main.ns').write_text('int Main() { T09InnerCallback q=t09_get_inner(); T09UnsupportedCallback callback = t09_get_unsupported_callback(); T09Unsupported v; v.x=(float)1; v.y=(float)2; v.z=(float)3; T09Unsupported r=callback(v,1,q); return r.x==(float)1 ? 0 : 1; }', encoding='utf-8')
command = [args.nsc, '--dump-project-oop-llvm', str(root / 'VectorCall.nsproj'),
           '--cpp-clang-root=' + args.clang_root, '--library-reference-root', str(root)]
result = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
(root / 'unsupported-record.log').write_text('EXIT: ' + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
if result.returncode == 0 or 'regcall function pointer signature is not yet supported' not in result.stdout:
    raise SystemExit('La firma indirecta de tres float no fue rechazada; evidencia: ' + str(root))
print('OK: conversiones incompatibles rechazadas; evidencia:', root)
