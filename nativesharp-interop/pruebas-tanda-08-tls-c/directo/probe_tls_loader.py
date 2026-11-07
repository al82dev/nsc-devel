import ctypes
import pathlib
import struct

root = pathlib.Path(r'A:\nsc-interop-build\tls-abi')
source = pathlib.Path(r'A:\nsc-interop-build\tanda08-tls-directo\scalars\api.dll')
target = root / 'tls_sentinel.dll'
data = bytearray(source.read_bytes())
u16 = lambda p: struct.unpack_from('<H', data, p)[0]
u32 = lambda p: struct.unpack_from('<I', data, p)[0]
u64 = lambda p: struct.unpack_from('<Q', data, p)[0]
pe = u32(0x3c)
optional = pe + 24
sections = optional + u16(pe + 20)
image_base = u64(optional + 24)
def file_offset(rva):
    for i in range(u16(pe + 6)):
        section = sections + 40 * i
        va, raw_size, raw = u32(section + 12), u32(section + 16), u32(section + 20)
        if va <= rva < va + raw_size:
            return raw + rva - va
    raise ValueError(f'RVA not file backed: {rva:#x}')
tls = file_offset(u32(optional + 112 + 9 * 8))
index_rva = u64(tls + 16) - image_base
index_file = file_offset(index_rva)
struct.pack_into('<I', data, index_file, 0xffffffff)
target.write_bytes(data)
module = ctypes.WinDLL(str(target))
index = ctypes.c_uint32.from_address(module._handle + index_rva).value
print(f'index_rva={index_rva:#x} index_after_load={index:#x}')
