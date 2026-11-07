import pathlib
import shutil
import struct

source = pathlib.Path(r'A:\nsc-interop-build\tanda08-tls-directo\scalars')
dest = pathlib.Path(r'A:\nsc-interop-build\tls-abi\zerofill')
dest.mkdir(parents=True, exist_ok=True)
data = bytearray((source / 'api.dll').read_bytes())
u16 = lambda p: struct.unpack_from('<H', data, p)[0]
u32 = lambda p: struct.unpack_from('<I', data, p)[0]
u64 = lambda p: struct.unpack_from('<Q', data, p)[0]
pe = u32(0x3c)
optional = pe + 24
image_base = u64(optional + 24)
sections = optional + u16(pe + 20)
def file_offset(rva):
    for i in range(u16(pe + 6)):
        s = sections + 40 * i
        va, length, raw = u32(s + 12), u32(s + 16), u32(s + 20)
        if va <= rva < va + length:
            return raw + rva - va
    raise ValueError(f'RVA not file backed: {rva:#x}')
tls = file_offset(u32(optional + 112 + 9 * 8))
start, end, old_zero = u64(tls), u64(tls + 8), u32(tls + 32)
assert old_zero == 0 and end - start >= 5
tail = file_offset(end - image_base - 5)
assert data[tail:tail + 5] == bytes(5)
struct.pack_into('<Q', data, tls + 8, end - 5)
struct.pack_into('<I', data, tls + 32, 5)
(dest / 'api.dll').write_bytes(data)
for name in ('crt-control.exe', 'crt-scalar-O0.exe', 'crt-scalar-O1.exe', 'crt-scalar-O2.exe', 'crt-scalar-O3.exe'):
    shutil.copy2(source / name, dest / name)
print(f'TLS raw {end - start} -> {end - start - 5}; zero-fill 0 -> 5')
