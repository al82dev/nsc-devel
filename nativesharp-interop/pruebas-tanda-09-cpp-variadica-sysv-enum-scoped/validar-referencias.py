"""Dos referencias C++ equivalentes y rechazo de un enum incompatible."""
import argparse
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
args = parser.parse_args()
fixture = Path(__file__).resolve().parent
root = Path(tempfile.mkdtemp(prefix='nsc-t09-sysv-enum-referencias-', dir='A:/temp'))
print('Evidencia:', root, flush=True)

def run(label, command, expected_success=True):
    result = subprocess.run(command, cwd=fixture.parents[1], stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    log = root / (label + '.log')
    log.write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
    if (result.returncode == 0) != expected_success:
        raise SystemExit(label + ': resultado inesperado; evidencia: ' + str(log))
    return result.stdout

for route, source in [('directa', fixture), ('indirecta', fixture.parent / (fixture.name + '-indirecta'))]:
    dest = root / route
    shutil.copytree(source, dest)
    (dest / 'api2.h').write_text('#pragma once\n#include "api.h"\n', encoding='utf-8')
    (dest / 'api2-dll.h').write_text('#pragma once\n#include "api-dll.h"\n', encoding='utf-8')
    for name in ('VectorCall.nsproj', 'VectorCallDll.nsproj'):
        p = dest / name
        data = p.read_text(encoding='utf-8')
        match = re.search(r'<CppLibraryReference [^>]*/>', data)
        if not match:
            raise SystemExit('falta referencia en ' + str(p))
        second = match.group(0).replace('Header="api', 'Header="api2')
        second = re.sub(r' (?:Library|Runtime)="[^"]*"', '', second)
        p.write_text(data.replace(match.group(0), match.group(0) + second), encoding='utf-8')
    p = dest / 'validar.py'
    data = p.read_text(encoding='utf-8')
    old = '("api.h", "api-dll.h", "api.cpp"'
    if old not in data:
        raise SystemExit('falta lista de archivos en ' + str(p))
    p.write_text(data.replace(old, '("api.h", "api2.h", "api2-dll.h", "api-dll.h", "api.cpp"'), encoding='utf-8')
    p = dest / 'validar-precompilado.py'
    data = p.read_text(encoding='utf-8')
    data = data.replace('("api.h", "api-dll.h", "api.cpp")', '("api.h", "api2.h", "api-dll.h", "api2-dll.h", "api.cpp")')
    data = data.replace('("api.h", "api-dll.h")', '("api.h", "api2.h", "api-dll.h", "api2-dll.h")')
    needle = "        producer_xml = ('<Project><PropertyGroup><Name>T09Producer</Name>'"
    if needle not in data:
        raise SystemExit('falta proyecto productor en ' + str(p))
    extra = "        cpp_ref += '<CppLibraryReference Header=\"' + ('api2-dll.h' if runtime else 'api2.h') + '\" Language=\"C++\" Standard=\"c++20\" />'\n"
    p.write_text(data.replace(needle, extra + needle), encoding='utf-8')
    command = [sys.executable, str(dest / 'validar.py'), '--nsc', args.nsc, '--clang-root', args.clang_root]
    output = run(route + '-vivo', command)
    match = re.search(r'^Evidencia: (.+)$', output, re.MULTILINE)
    if not match:
        raise SystemExit('falta ruta de evidencia en ' + route)
    if route == 'directa':
        negative = root / 'conflicto'
        shutil.copytree(Path(match.group(1).strip()) / 'estatica-O0', negative)
        (negative / 'api2.h').write_text('#pragma once\nnamespace T09Cpp { enum class Mode : unsigned short { Chosen=65534, Changed=11 }; }\n', encoding='utf-8')
        command = [args.nsc, '--dump-project-oop-llvm', str(negative / 'VectorCall.nsproj'),
                   '--library-reference-root', str(negative), '--cpp-clang-root=' + args.clang_root]
        diagnostic = run('conflicto', command, expected_success=False)
        if 'conflicting C enum declarations: Mode' not in diagnostic:
            raise SystemExit('el rechazo no identifica el enum; evidencia: ' + str(root / 'conflicto.log'))
    run(route + '-precompilado', [sys.executable, str(dest / 'validar-precompilado.py'),
                                '--nsc', args.nsc, '--clang-root', args.clang_root])
    print(route, 'OK', flush=True)
print('OK: referencias equivalentes y rechazo de conflicto; evidencia:', root)
