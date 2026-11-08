from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-tls-host-", dir=r"A:\nsc-interop-build"))
clang = Path(r"A:\al82dev-clang\bin\clang.exe")
lld = Path(r"A:\al82dev-clang\bin\lld-link.exe")
readobj = Path(r"A:\al82dev-clang\bin\llvm-readobj.exe")
source = Path(__file__).resolve().parent.parent / "tls-dll-distintas" / "TlsApi2SinImport.c"
libs = [r"A:\vc2026-runtime\lib\x64\release\libcmt.lib",
        r"A:\vc2026-runtime\lib\x64\release\libcpmt.lib",
        r"A:\vc2026-runtime\lib\x64\release\libvcruntime.lib",
        r"A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\ucrt\x64\libucrt.lib",
        r"A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib"]

def run(command):
    result = subprocess.run([str(item) for item in command], cwd=root, text=True,
                            encoding="utf-8", errors="replace", stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT)
    if result.returncode:
        raise AssertionError(str(command) + "\n" + result.stdout)
    return result.stdout

probe = '''import ctypes, json, sys, threading
d = ctypes.WinDLL(sys.argv[1]); values = [d.tls_read()]
def reader(): values.append(d.tls_read())
t = threading.Thread(target=reader); t.start(); t.join()
print(json.dumps(values))
'''
for imported in (False, True):
    name = "with-import" if imported else "without-import"
    obj = root / (name + ".obj")
    dll = root / (name + ".dll")
    command = [clang, "-c", "--target=x86_64-pc-windows-msvc", str(source), "-o", obj]
    if imported: command.append("-DTEST_UNUSED_IMPORT")
    run(command)
    run([lld, "/nologo", "/dll", "/noentry", "/nodefaultlib", "/include:_tls_used",
         "/export:tls_exported,DATA", "/out:" + str(dll), obj] + libs)
    imports = run([readobj, "--coff-imports", dll])
    if ("GetCurrentThreadId" in imports) != imported:
        raise AssertionError("tabla de importación inesperada: " + imports)
    observed = run([sys.executable, "-c", probe, dll]).strip()
    print(name, observed, flush=True)
    expected = "[7, 7]" if imported else "[-2147483648, -2147483648]"
    if observed != expected:
        raise AssertionError("cambió el comportamiento observado en el host estándar")
print("Comportamiento TLS reproducido en host estándar;", root)
