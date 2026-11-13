"""Comprueba records C++ compactos como argumentos cdecl variadicos con va_arg real."""
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
root = Path(tempfile.mkdtemp(prefix='nsc-t09-cpp-cdecl-record-compactos-', dir=r'A:\temp'))
env = os.environ.copy()
env['TEMP'] = env['TMP'] = str(root)
for name, cpp_type, ns_type in (('1-byte','unsigned char','byte'), ('2-bytes','unsigned short','ushort'),
                                ('4-bytes','int','int'), ('8-bytes','long long','long')):
    case = root / name
    case.mkdir()
    for filename in ('api.h','api-dll.h','api.cpp','Control.cpp','Main.ns','Producer.ns',
                     'Consumer.ns','VectorCall.nsproj','VectorCallDll.nsproj','validar.py','validar-precompilado.py'):
        shutil.copyfile(fixture / filename, case / filename)
    header = case / 'api.h'
    header.write_text(header.read_text(encoding='utf-8').replace('double x; int y;', cpp_type + ' x;'), encoding='utf-8')
    producer = case / 'api.cpp'
    producer.write_text(producer.read_text(encoding='utf-8').replace('value.x==2.25 && value.y==7','value.x==7'), encoding='utf-8')
    (case / 'Control.cpp').write_text('#include "api.h"\nextern "C" { __declspec(selectany) int _fltused = 0; }\n'
        'int main() { T09VarValue v = {7}; return T09Cpp::sum(1,v)==32 && v.x==7 ? 0 : 1; }\n', encoding='utf-8')
    expr = 'T09VarValue v; v.x=(' + ns_type + ')7; return T09Cpp.sum(1,v)==32 && v.x==7 ? 0 : 1;'
    (case / 'Main.ns').write_text('int Main() { ' + expr + ' }\n', encoding='utf-8')
    (case / 'Producer.ns').write_text('namespace T09 { public class RegBridge { public static int Run<T>(T unused) { ' + expr + ' } } }\n', encoding='utf-8')
    for script in ('validar.py', 'validar-precompilado.py'):
        command = ['python.exe', str(case / script), '--nsc', args.nsc, '--clang-root', args.clang_root]
        result = subprocess.run(command, cwd=case, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
        (root / (name + '-' + script + '.log')).write_text(repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
        if result.returncode or 'OK:' not in result.stdout:
            print(result.stdout, flush=True)
            raise SystemExit(name + ': fallo ' + script + '; evidencia: ' + str(root))
    print('OK:', name, 'directo y precompilado', flush=True)
print('Evidencia:', root)