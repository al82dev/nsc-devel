"""Contrasta copias PE sin cambiar la DLL original ni las pruebas de la tanda."""
import ctypes
import pathlib
import struct
import threading

source = pathlib.Path(r'A:\nsc-interop-build\tanda08-tls-directo\scalars\api.dll')
out = pathlib.Path(r'A:\nsc-interop-build\tls-abi')
original = source.read_bytes()
u16 = lambda b, p: struct.unpack_from('<H', b, p)[0]
u32 = lambda b, p: struct.unpack_from('<I', b, p)[0]
u64 = lambda b, p: struct.unpack_from('<Q', b, p)[0]
pe = u32(original, 0x3c)
optional = pe + 24
sections = optional + u16(original, pe + 20)
image_base = u64(original, optional + 24)

def offset(rva):
    for i in range(u16(original, pe + 6)):
        section = sections + 40 * i
        va, size, raw = u32(original, section + 12), u32(original, section + 16), u32(original, section + 20)
        if va <= rva < va + size:
            return raw + rva - va
    raise ValueError(f'RVA outside file-backed section: {rva:#x}')

directory = optional + 112
if u32(original, directory + 8) != 0:
    raise ValueError('Expected the DLL fixture without imports')
tls = offset(u32(original, directory + 9 * 8))
index_rva = u64(original, tls + 16) - image_base
padding_rva = 0x2300
padding_offset = offset(padding_rva)
if original[padding_offset:padding_offset + 20] != bytes(20):
    raise ValueError('Expected a zero-filled import-descriptor area')
indices = {}
for variant in ('absent', 'empty'):
    data = bytearray(original)
    struct.pack_into('<I', data, offset(index_rva), 0xffffffff)
    if variant == 'empty':
        struct.pack_into('<II', data, directory + 8, padding_rva, 20)
    target = out / ('tls_import_' + variant + '.dll')
    target.write_bytes(data)
    module = ctypes.WinDLL(str(target))
    index = ctypes.c_uint32.from_address(module._handle + index_rva).value
    indices[variant] = index
    print(f'{variant}: module={module._handle:#x}, index={index:#x}', flush=True)
    if variant == 'empty' and index != 0xffffffff:
        read = module.tls_zero_read
        read.restype = ctypes.c_int
        values = [read()]
        worker = threading.Thread(target=lambda: values.append(read()))
        worker.start()
        worker.join()
        if values != [0, 0]:
            raise SystemExit(f'Lectura TLS inesperada: {values!r}')
        print('empty: lectura TLS cero en ambos hilos', flush=True)
if indices['absent'] != 0xffffffff or indices['empty'] == 0xffffffff:
    raise SystemExit('El resultado difiere de la reproduccion observada')
