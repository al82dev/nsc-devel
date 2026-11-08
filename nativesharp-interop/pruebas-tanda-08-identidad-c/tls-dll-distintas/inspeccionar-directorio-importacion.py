from pathlib import Path
import shutil
import struct
import subprocess
import sys

fixtures = Path(__file__).resolve().parent
source = Path(r'A:\nsc-interop-build\t08-tls-dos-dll-repro\origen-sin-import')
work = Path(r'A:\nsc-interop-build\t08-tls-directorio-importacion')
result = subprocess.run([sys.executable, str(fixtures / 'inspeccionar-tls.py')],
                        capture_output=True, text=True, errors='replace')
if result.returncode:
    sys.exit(result.stdout + result.stderr)
work.mkdir(parents=True, exist_ok=True)
for name in ('api.dll', 'inspect.exe', 'dynamic.exe'):
    shutil.copy2(source / name, work / name)

def observe(expect_initialized, expected_dynamic):
    inspected = subprocess.run([str(work / 'inspect.exe')], cwd=work,
                               capture_output=True, text=True, errors='replace')
    if inspected.returncode:
        sys.exit(inspected.stdout + inspected.stderr)
    values = dict(line.split('=', 1) for line in inspected.stdout.splitlines())
    if (int(values['read'], 16) == 7) != expect_initialized:
        sys.exit('TLS inesperado: ' + inspected.stdout)
    dynamic = subprocess.run([str(work / 'dynamic.exe')], cwd=work)
    if dynamic.returncode != expected_dynamic:
        sys.exit('Carga dinamica inesperada: ' + str(dynamic.returncode))
    print(inspected.stdout.strip(), 'dynamic=', dynamic.returncode, flush=True)

print('DLL original sin directorio de importacion:', flush=True)
observe(False, 106)

# PE de prueba: descriptor nulo en espacio libre de .rdata; no modifica la DLL original.
path = work / 'api.dll'
data = bytearray(path.read_bytes())
nt = struct.unpack_from('<I', data, 0x3c)[0]
count = struct.unpack_from('<H', data, nt + 6)[0]
optional_size = struct.unpack_from('<H', data, nt + 20)[0]
optional = nt + 24
if struct.unpack_from('<H', data, optional)[0] != 0x20b:
    sys.exit('Se esperaba PE32+')
imports = optional + 112 + 8
if struct.unpack_from('<II', data, imports) != (0, 0):
    sys.exit('La DLL original ya tiene directorio de importacion')
for i in range(count):
    section = optional + optional_size + i * 40
    if bytes(data[section:section + 8]).rstrip(b'\0') != b'.rdata':
        continue
    size, va, raw_size, raw = struct.unpack_from('<IIII', data, section + 8)
    offset = (size + 7) & ~7
    if offset + 20 > raw_size or data[raw + offset:raw + offset + 20] != bytes(20):
        sys.exit('Sin espacio cero para el descriptor de control')
    struct.pack_into('<I', data, section + 8, offset + 20)
    struct.pack_into('<II', data, imports, va + offset, 20)
    path.write_bytes(data)
    break
else:
    sys.exit('Falta la seccion .rdata')

print('Copia con descriptor de importacion vacio:', flush=True)
observe(True, 0)
