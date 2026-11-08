from pathlib import Path
import sys

path = Path(sys.argv[1])
data = bytearray(path.read_bytes())
if len(data) < 44 or data[:8] != b'NSTHINLT' or data[8:12] != b'\x03\x00\x00\x00':
    raise SystemExit('Se esperaba un sidecar ThinLTO v3 completo')
data[-1] ^= 1
path.write_bytes(data)
