import ctypes
import pathlib
import struct
import subprocess
import sys

folder = pathlib.Path(r'A:\nsc-interop-build\t08-tls-contraste-import')
u16 = lambda data, pos: struct.unpack_from('<H', data, pos)[0]
u32 = lambda data, pos: struct.unpack_from('<I', data, pos)[0]
u64 = lambda data, pos: struct.unpack_from('<Q', data, pos)[0]

def headers(data):
    nt = u32(data, 0x3c)
    opt = nt + 24
    section_start = opt + u16(data, nt + 20)
    sections = []
    for i in range(u16(data, nt + 6)):
        p = section_start + i * 40
        sections.append((bytes(data[p:p+8]).rstrip(b'\0'), u32(data,p+12),
                         u32(data,p+8), u32(data,p+20), u32(data,p+16)))
    return opt, sections

def file_offset(sections, rva):
    for _, va, _, raw, size in sections:
        if va <= rva < va + size:
            return raw + rva - va
    raise ValueError('RVA outside raw sections')

if len(sys.argv) > 1:
    image = ctypes.WinDLL(str(folder / sys.argv[1]))
    base = image._handle
    nt = base + ctypes.c_uint32.from_address(base + 0x3c).value
    tls_rva = ctypes.c_uint32.from_address(nt + 208).value
    index_ptr = ctypes.c_void_p.from_address(base + tls_rva + 16).value
    index = ctypes.c_uint32.from_address(index_ptr).value
    image.tls_zero_read.restype = ctypes.c_int
    print(f'{sys.argv[1]} index={index:#x}', flush=True)
    print(f'read={image.tls_zero_read()}', flush=True)
    sys.exit(0)

original = bytearray((folder / 'noimport.dll').read_bytes())
opt, sections = headers(original)
assert u32(original, opt + 112 + 8) == 0
tls = file_offset(sections, u32(original, opt + 112 + 9 * 8))
index_rva = u64(original, tls + 16) - u64(original, opt + 24)
index_file = file_offset(sections, index_rva)
assert u32(original, index_file) == 0
struct.pack_into('<I', original, index_file, 0xffffffff)
(folder / 'control-sentinel.dll').write_bytes(original)

modified = bytearray(original)
for name, va, virtual_size, raw, raw_size in sections:
    if name == b'.rdata':
        # Last 20 bytes of raw padding are outside initialized section contents.
        assert raw_size - virtual_size >= 20
        assert not any(modified[raw + raw_size - 20:raw + raw_size])
        descriptor = va + raw_size - 20
        break
else:
    raise ValueError('missing .rdata')
struct.pack_into('<II', modified, opt + 112 + 8, descriptor, 20)
(folder / 'empty-import-sentinel.dll').write_bytes(modified)
for name in ('control-sentinel.dll', 'empty-import-sentinel.dll'):
    for iteration in range(5):
        completed = subprocess.run([sys.executable, __file__, name], text=True, capture_output=True)
        print(name, ' run=', iteration, ' exit=', completed.returncode,
              ' ', completed.stdout.replace('\n', ' '), sep='')
