import ctypes
import pathlib
import sys

dll = ctypes.WinDLL(str(pathlib.Path(sys.argv[1]).resolve()))
check = getattr(dll, '__nsc_method_LinkLibTls$3A$3ALinkLib$2EBridge__Check')
check.restype = ctypes.c_int
value = check()
print(f'DLL TLS isolation result: {value}', flush=True)
if value != 0:
    raise SystemExit(1)
