from pathlib import Path
import os
import shutil
import subprocess
import sys

root = Path(r"A:\nsc-interop-build\t08-tls-dos-dll-repro")
upper = root / "ref-\u00c9.lib"
lower = root / "ref-\u00e9.lib"
distinct = root / "ref-\u00ca.lib"
shutil.copyfile(root / "TlsProducer.lib", upper)
shutil.copyfile(root / "TlsProducer.lib", distinct)
if not os.path.samefile(upper, lower) or os.path.samefile(upper, distinct):
    raise RuntimeError("El sistema de archivos no ofrece la equivalencia necesaria para esta prueba")

project = root / "unicode-case.nsproj"
def check(second):
    project.write_text(
        '<Project><PropertyGroup><Name>CaseProbe</Name></PropertyGroup><ItemGroup>'
        '<Compile Include="ConsumerCache.ns" />'
        '<LibraryReference Include="ref-\u00c9.lib" Metadata="TlsProducer.nsmeta" />'
        '<LibraryReference Include="' + second + '" Metadata="TlsProducer.nsmeta" />'
        '</ItemGroup></Project>', encoding="utf-8")
    return subprocess.run(["nsc.exe", "--check-project", str(project)], cwd=root,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                          text=True, encoding="utf-8", errors="replace")

duplicate = check("ref-\u00e9.lib")
if duplicate.returncode == 0 or "duplicate LibraryReference" not in duplicate.stdout or not any(
        spelling in duplicate.stdout for spelling in ("ref-\u00c9.lib", "ref-\u00e9.lib")):
    print("ERROR: la ruta equivalente no se diagnostico como duplicada con su nombre Unicode")
    print(duplicate.stdout.encode("ascii", "backslashreplace").decode())
    sys.exit(1)
separate = check("ref-\u00ca.lib")
if separate.returncode != 0:
    print("ERROR: las rutas distintas se trataron como duplicadas")
    print(separate.stdout.encode("ascii", "backslashreplace").decode())
    sys.exit(1)
named_project = root / "Proyecto-\u6f22.nsproj"
shutil.copyfile(project, named_project)
named = subprocess.run(["nsc.exe", "--check-project", str(named_project)], cwd=root,
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                       text=True, encoding="utf-8", errors="replace")
if named.returncode != 0:
    print("ERROR: un proyecto con nombre de archivo Unicode y Name explicito fallo")
    print(named.stdout.encode("ascii", "backslashreplace").decode())
    sys.exit(1)
print("Identidad de biblioteca con mayusculas Unicode: correcta.")
