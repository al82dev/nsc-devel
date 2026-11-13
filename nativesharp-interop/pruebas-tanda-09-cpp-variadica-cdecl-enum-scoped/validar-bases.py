"""Compara bases de enum class variadico cdecl directo e indirecto con Clang."""
import argparse
import re
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--nsc', required=True)
parser.add_argument('--clang-root', required=True)
parser.add_argument('--base', choices=('i8', 'u8', 'i16', 'u16', 'i32', 'u32', 'i64', 'u64'))
parser.add_argument('--ruta', choices=('directa', 'indirecta'))
args = parser.parse_args()
fixture = Path(__file__).resolve().parent
sources = {'directa': fixture, 'indirecta': fixture.parent / (fixture.name + '-indirecta')}
root = Path(tempfile.mkdtemp(prefix='nsc-t09-cpp-enum-scoped-bases-', dir='A:/temp'))
print('Evidencia:', root, flush=True)
cases = [('i8', 'signed char', '-100', 8), ('u8', 'unsigned char', '255', 8),
         ('i16', 'short', '-30000', 16), ('u16', 'unsigned short', '65535', 16),
         ('i32', 'int', '-2000000000', 32), ('u32', 'unsigned int', '4000000000U', 32),
         ('i64', 'long long', '4294967297LL', 64),
         ('u64', 'unsigned long long', '18446744073709551615ULL', 64)]

def run(case):
    route, (name, base, value, bits) = case
    dest = root / (route + '-' + name)
    shutil.copytree(sources[route], dest)
    p = dest / 'api.h'
    data = p.read_text(encoding='utf-8')
    old = 'enum class Mode : unsigned short'
    if old not in data or 'Chosen=65535' not in data:
        raise RuntimeError('fixture inesperado: ' + str(p))
    data = data.replace(old, 'enum class Mode : ' + base).replace('Chosen=65535', 'Chosen=' + value)
    p.write_text(data, encoding='utf-8')
    p = dest / 'validar.py'
    data = p.read_text(encoding='utf-8')
    if 'i16 [^,]+' not in data:
        raise RuntimeError('falta asercion de IR: ' + str(p))
    data = data.replace('i16 [^,]+', 'i' + str(bits) + ' [^,]+').replace('ancho i16', 'ancho i' + str(bits))
    p.write_text(data, encoding='utf-8')
    control_ir = dest / 'control-clang.ll'
    command = ['clang.exe', '-target', 'x86_64-pc-windows-msvc', '-std=c++20', '-O0',
               '-S', '-emit-llvm', str(dest / 'Control.cpp'), '-o', str(control_ir)]
    result = subprocess.run(command, cwd=fixture.parents[1], stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace')
    ir = control_ir.read_text(encoding='utf-8') if result.returncode == 0 else ''
    pattern = r'call[^\n]*i32 \(i32, \.\.\.\)[^\n]*\(i32[^,]*, i' + str(bits) + r'[^,]*, i32[^)]*\)'
    verified = result.returncode == 0 and bool(re.search(pattern, ir))
    log = root / (route + '-' + name + '-clang-ir.log')
    log.write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) +
                   '\nCHECK: ' + str(verified) + '\n' + result.stdout, encoding='utf-8')
    if not verified:
        return route, name, str(log)
    for script in ('validar.py', 'validar-precompilado.py'):
        command = [sys.executable, str(dest / script), '--nsc', args.nsc, '--clang-root', args.clang_root]
        result = subprocess.run(command, cwd=fixture.parents[1], stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, text=True, errors='replace')
        log = root / (route + '-' + name + '-' + script + '.log')
        log.write_text('COMMAND: ' + repr(command) + '\nEXIT: ' + str(result.returncode) + '\n' + result.stdout, encoding='utf-8')
        if result.returncode:
            return route, name, str(log)
    return route, name, None

failures = []
with ThreadPoolExecutor(max_workers=3) as pool:
    for future in as_completed([pool.submit(run, (route, case)) for route in sources for case in cases if (not args.ruta or route == args.ruta) and (not args.base or case[0] == args.base)]):
        route, name, error = future.result()
        print(route, name, 'fallo: ' + error if error else 'OK', flush=True)
        if error:
            failures.append(error)
if failures:
    raise SystemExit('Fallos: ' + ', '.join(failures))
print('OK: IR de Clang y NSC y consumidores precompilados; evidencia:', root)
