from pathlib import Path
import subprocess
import sys

root = Path(sys.argv[1])
archive = bytearray((root / "long.lib").read_bytes())
if archive[:8] != b"!<arch>\n":
    raise SystemExit("Archivo COFF inválido")
offset = 8
while offset + 60 <= len(archive):
    size = int(archive[offset + 48:offset + 58].decode("ascii").strip())
    start = offset + 60
    if archive[start:start + 2] == b"\x64\x86":
        count = int.from_bytes(archive[start + 2:start + 4], "little")
        optional = int.from_bytes(archive[start + 16:start + 18], "little")
        for section in range(count):
            sh = start + 20 + optional + section * 40
            if archive[sh:sh + 8] != b".idata$2":
                continue
            relocs = start + int.from_bytes(archive[sh + 24:sh + 28], "little")
            nrelocs = int.from_bytes(archive[sh + 32:sh + 34], "little")
            for index in range(nrelocs):
                entry = relocs + index * 10
                if int.from_bytes(archive[entry:entry + 4], "little") == 12:
                    archive[entry + 4:entry + 8] = b"\xff\xff\xff\xff"
                    break
            else:
                raise SystemExit("No se encontró la relocalización Name")
            break
        else:
            offset = start + size + (size & 1)
            continue
        break
    offset = start + size + (size & 1)
else:
    raise SystemExit("No se encontró el descriptor COFF")
(root / "malformed.lib").write_bytes(archive)
project = (root / "Project.nsproj").read_text(encoding="utf-8")
(root / "Malformed.nsproj").write_text(project.replace('Library="long.lib"',
    'Library="malformed.lib"'), encoding="utf-8")
result = subprocess.run([
    r"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe", "--emit-project-oop-exe",
    str(root / "Malformed.nsproj"), "-O0", "--no-artifact-cache",
    "--cpp-clang-root=A:\\al82dev-clang", "-o", str(root / "malformed.exe")
], capture_output=True, text=True)
if result.returncode == 0 or "COFF long import has an invalid name relocation" not in result.stderr:
    print(result.stdout, result.stderr)
    raise SystemExit("No se rechazó el descriptor COFF malformado")
print("Descriptor COFF largo malformado rechazado.")
