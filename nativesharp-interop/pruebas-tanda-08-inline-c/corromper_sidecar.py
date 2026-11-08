from pathlib import Path
import hashlib
import struct
import sys

path = Path(sys.argv[1])
data = bytearray(path.read_bytes())
if len(data) < 44 or data[:8] != b'NSTHINLT' or data[8:12] != b'\x03\x00\x00\x00':
    raise SystemExit('Se esperaba un sidecar ThinLTO v3 completo')
if len(sys.argv) == 2:
    data[-1] ^= 1
elif len(sys.argv) == 3 and sys.argv[2] == 'objeto':
    offset = 12
    for _ in range(4):
        size, = struct.unpack_from('<I', data, offset)
        offset += 4 + size
    count, = struct.unpack_from('<I', data, offset)
    offset += 4
    for _ in range(count):
        size, = struct.unpack_from('<Q', data, offset)
        offset += 8 + size
    objects, = struct.unpack_from('<I', data, offset)
    offset += 4
    if objects < 1:
        raise SystemExit('El sidecar no contiene objetos nativos')
    size, = struct.unpack_from('<Q', data, offset)
    offset += 8
    if size < 2 or offset + size > len(data) - 32:
        raise SystemExit('Objeto nativo truncado')
    data[offset:offset + 2] = b'\x00\x00'
    data[-32:] = hashlib.sha256(data[:-32]).digest()
else:
    raise SystemExit('Modo de corrupcion desconocido')
path.write_bytes(data)
