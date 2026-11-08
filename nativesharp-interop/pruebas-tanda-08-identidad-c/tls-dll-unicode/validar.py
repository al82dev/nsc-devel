from pathlib import Path
import shutil
import subprocess
import sys

sys.stdout.reconfigure(errors="backslashreplace")
root = Path(r"A:\nsc-interop-build\t08-tls-dos-dll-repro")
fixture = Path(__file__).resolve().parent
name = "api-\u6f22"
output = root / ("salida-ascii" if "--ascii-output" in sys.argv else "salida-\u6f22")
output.mkdir(exist_ok=True)


def run(args, cwd=root):
    result = subprocess.run(args, cwd=cwd, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace")
    if result.returncode:
        print(result.stdout)
        print("salidas:", [p.name.encode("unicode_escape").decode("ascii") for p in output.iterdir()])
        print("fallo:", " ".join(args).encode("unicode_escape").decode("ascii"), "codigo", result.returncode)
        sys.exit(1)
    return result.stdout


run(["lld-link.exe", "/nologo", "/dll", "/noentry", "/nodefaultlib", "/include:_tls_used",
     "/export:tls_exported,DATA", "/out:" + name + ".dll", "/implib:" + name + ".lib",
     "TlsApi2.obj", r"A:\vc2026-runtime\lib\x64\release\libcmt.lib",
     r"A:\vc2026-runtime\lib\x64\release\libcpmt.lib",
     r"A:\vc2026-runtime\lib\x64\release\libvcruntime.lib",
     r"A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\ucrt\x64\libucrt.lib",
     r"A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib"])
project = (root / "Producer2.nsproj").read_text(encoding="utf-8")
project = project.replace("api2.lib", name + ".lib").replace("api2.dll", name + ".dll")
(root / "ProducerUnicode.nsproj").write_text(project, encoding="utf-8")
shutil.copyfile(fixture / ".." / ".." / "pruebas-tanda-08-tls-c" / "directo" / "TlsApi.h", root / "TlsApi.h")
for item in ("ConsumerCache.ns", "ConsumerCache.nsproj"):
    shutil.copyfile(fixture / ".." / "tls-dll-distintas" / item, root / item)
run(["nsc.exe", "--cpp-clang-root=A:\\al82dev-clang", "--emit-project-oop-lib",
     "ProducerUnicode.nsproj", "-O0", "--no-artifact-cache", "-o", "TlsProducer2.lib"])
dll_project = project.replace("<Name>TlsProducer2</Name>", "<Name>TlsUnicodeDll</Name>")
dll_project = dll_project.replace("<OutputType>Library</OutputType>", "<OutputType>SharedLibrary</OutputType>")
(root / "ProducerUnicodeDll.nsproj").write_text(dll_project, encoding="utf-8")
run(["nsc.exe", "--cpp-clang-root=A:\\al82dev-clang", "--emit-project-oop-dll",
     "ProducerUnicodeDll.nsproj", "-O0", "--time", "--no-artifact-cache", "-o", str(output / "UnicodeBridge.dll")])
(root / "TlsApi.h").unlink()
items = ("ConsumerCache.exe", "api.dll", name + ".dll")
cached = "--cache" in sys.argv
for item in items:
    (output / item).unlink(missing_ok=True)
for attempt in range(2 if cached else 1):
    cache_option = "--artifact-cache=A:\\nsc-interop-build\\t08-tls-unicode-cache" if cached else "--no-artifact-cache"
    report = run(["nsc.exe", "--cpp-clang-root=A:\\al82dev-clang", "--emit-project-oop-exe",
                  "ConsumerCache.nsproj", "-O0", "--time", cache_option, "-o", str(output / "ConsumerCache.exe")])
    if attempt == 1 and "compilation-bundle-cache: hit" not in report:
        print("ERROR: no se restauro la compilacion desde cache.")
        print(report)
        sys.exit(1)
    for item in items:
        if not (output / item).is_file():
            print("falta en salida:", item.encode("unicode_escape").decode("ascii"))
            sys.exit(1)
    run([str(output / "ConsumerCache.exe")], cwd=Path("A:/"))
    if cached and attempt == 0:
        for item in items:
            (output / item).unlink()
print("DLL Unicode y salida", "ASCII" if "--ascii-output" in sys.argv else "Unicode", ": correctas.")
