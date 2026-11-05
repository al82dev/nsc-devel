from pathlib import Path
import subprocess
import sys

root = Path(sys.argv[1])
archive = bytearray((root / "api.lib").read_bytes())
if archive[:8] != b"!<arch>\n":
    raise SystemExit("No es una biblioteca COFF")
offset = 8
while offset + 60 <= len(archive):
    size = int(archive[offset + 48:offset + 58].decode("ascii").strip())
    start = offset + 60
    if size >= 20 and archive[start:start + 6] == b"\0\0\xff\xff\0\0":
        archive[start + 12:start + 16] = b"\xff\xff\xff\xff"
        break
    offset = start + size + (size & 1)
else:
    raise SystemExit("No se encontró una entrada de importación corta")
(root / "malformed.lib").write_bytes(archive)
project = (root / "StaticRuntimeProducer.nsproj").read_text(encoding="utf-8")
(root / "MalformedRuntimeProducer.nsproj").write_text(
    project.replace('Library="api.lib"', 'Library="malformed.lib"'), encoding="utf-8")
result = subprocess.run([
    r"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe", "--emit-project-oop-lib",
    str(root / "MalformedRuntimeProducer.nsproj"), "-O0", "--no-artifact-cache",
    "--cpp-clang-root=A:\\al82dev-clang", "-o", str(root / "package" / "Malformed.lib")
], capture_output=True, text=True)
if result.returncode == 0 or "COFF short import has invalid data size" not in result.stderr:
    print(result.stdout, result.stderr)
    raise SystemExit("No se rechazó la entrada COFF malformada")
print("Entrada COFF malformada rechazada.")
