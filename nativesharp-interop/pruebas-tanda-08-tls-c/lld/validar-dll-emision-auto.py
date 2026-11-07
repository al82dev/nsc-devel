import ctypes
import pathlib
import sys

path = pathlib.Path(sys.argv[1]).resolve()
dll = ctypes.WinDLL(str(path))
prefix = '__nsc_method_TlsSharedProducer$3A$3APublishedTls$2EBridge__'
read = getattr(dll, prefix + 'Read')
write = getattr(dll, prefix + 'Write__builtin$3Aint')
worker = getattr(dll, prefix + 'CheckWorker')
read.restype = ctypes.c_int
write.argtypes = [ctypes.c_int]
write.restype = None
worker.restype = ctypes.c_int
first = read()
write(41)
second = read()
worker_result = worker()
last = read()
print(f'initial={first} written={second} worker={worker_result} final={last}', flush=True)
if (first, second, worker_result, last) != (3, 41, 0, 41):
    raise SystemExit(1)
