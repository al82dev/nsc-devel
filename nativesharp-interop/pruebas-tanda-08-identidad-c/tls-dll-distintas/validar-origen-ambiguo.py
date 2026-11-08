from pathlib import Path
import os
import shutil
import subprocess
import sys

fixtures = Path(__file__).resolve().parent
root = Path(r"A:\nsc-interop-build\t08-tls-dos-dll-repro")
without_imports = "--sin-import" in sys.argv
with_entry = "--entrada" in sys.argv
unused_import = "--import-sin-llamar" in sys.argv
work = root / ("origen-import-sin-llamar" if unused_import else
               "origen-sin-import-entrada" if with_entry else
               "origen-sin-import" if without_imports else "origen-ambiguo")
work.mkdir(parents=True, exist_ok=True)
for name in ("TlsApi2Ambiguo.c", "TlsApi2SinImport.c", "ConsumerAmbiguo.ns", "ConsumerAmbiguo.nsproj",
             "Producer2Ambiguo.nsproj", "Producer2.ns", "ReadApi.h"):
    shutil.copyfile(fixtures / name, work / name)
shutil.copyfile(fixtures.parents[1] / "pruebas-tanda-08-tls-c" / "directo" / "TlsApi.h", work / "TlsApi.h")
env = os.environ.copy()
env["PATH"] = r"A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;" + env["PATH"]
env["NSC_LINKER"] = r"A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
env["NSC_LLVM_LIB"] = r"A:\al82dev-clang\bin\llvm-lib.exe"
env["NSC_WINDOWS_RUNTIME_LIBRARY"] = r"A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
clang = r"A:\al82dev-clang\bin\clang.exe"
lld = r"A:\al82dev-clang\bin\lld-link.exe"
nsc = r"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe"
sdk = r"A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib"
crt = "A:\\vc2026-runtime\\lib\\x64\\release\\"
def run(command):
    result = subprocess.run(command, cwd=work, env=env, capture_output=True, text=True, errors="replace")
    if result.returncode:
        print(result.stdout + result.stderr)
        print("ERROR: codigo", result.returncode, "al ejecutar", command[0])
        sys.exit(result.returncode)

source = "TlsApi2SinImport.c" if without_imports else "TlsApi2Ambiguo.c"
compile_dll = [clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11"]
if with_entry:
    compile_dll.append("-DTEST_DLL_ENTRY")
if unused_import:
    compile_dll.append("-DTEST_UNUSED_IMPORT")
run(compile_dll + [source, "-o", "api.obj"])
run([lld, "/nologo", "/dll", "/entry:DllMain" if with_entry else "/noentry", "/nodefaultlib", "/include:_tls_used", "/export:tls_exported,DATA",
     "/out:api.dll", "/implib:api.lib", "api.obj", crt + "libcmt.lib", crt + "libcpmt.lib",
     crt + "libvcruntime.lib", r"A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\ucrt\x64\libucrt.lib", sdk])
probe = (fixtures.parents[1] / "pruebas-tanda-08-tls-c" / "directo" / "ProbePeTls.c").read_text(encoding="utf-8")
probe = probe.replace("!= 3", "!= 7")
probe = probe.replace("if (!slot || *slot != 7 || tls_read() != 7) ExitProcess(1);",
                      "if (!slot) ExitProcess(101); if (tls_read() != 7) ExitProcess(103); if (*slot != 7) ExitProcess(102);")
(work / "ProbeAmbiguo.c").write_text(probe, encoding="utf-8")
run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "ProbeAmbiguo.c", "-o", "ProbeAmbiguo.obj"])
run([lld, "/nologo", "/entry:mainCRTStartup", "/subsystem:console", "/nodefaultlib", "/out:ProbeAmbiguo.exe",
     "ProbeAmbiguo.obj", "api.lib", sdk])
if without_imports:
    result = subprocess.run([str(work / "ProbeAmbiguo.exe")], cwd=work, env=env)
    if unused_import:
        print("Control C con importacion no invocada: codigo", result.returncode)
        sys.exit(result.returncode)
    if with_entry:
        print("Control C sin importaciones, con DllMain: codigo", result.returncode)
        if result.returncode != 103:
            sys.exit("ERROR: el resultado del control con entrada cambio")
        sys.exit(0)
    if result.returncode != 103:
        sys.exit("ERROR: el resultado del control sin importaciones cambio: " + str(result.returncode))
    print("Control C sin importaciones: tls_read no observa el valor inicial; pendiente de investigar")
    sys.exit(0)
run([str(work / "ProbeAmbiguo.exe")])
if "--solo-c" in sys.argv:
    print("Control C con importacion: valor TLS inicial correcto")
    sys.exit(0)
run([nsc, "--cpp-clang-root=A:\\al82dev-clang", "--emit-project-oop-lib", "Producer2Ambiguo.nsproj",
     "-O0", "--no-artifact-cache", "-o", "TlsProducer2.lib"])
(work / "ConsumerAmbiguo.exe").unlink(missing_ok=True)
result = subprocess.run([nsc, "--cpp-clang-root=A:\\al82dev-clang", "--emit-project-oop-exe",
                         "ConsumerAmbiguo.nsproj", "-O0", "--no-artifact-cache", "-o", "ConsumerAmbiguo.exe"],
                        cwd=work, env=env, capture_output=True, text=True, errors="replace")
if result.returncode == 0:
    sys.exit("ERROR: NSC selecciono una DLL pese a dos origenes para tls_read")
log = result.stdout + result.stderr
(work / "diagnostico.log").write_text(log, encoding="utf-8")
if "same-named C TLS DLLs are also imported directly and cannot yet be distinguished" not in log:
    print(log)
    sys.exit("ERROR: fallo por una causa distinta de la ambiguedad")
if (work / "ConsumerAmbiguo.exe").exists():
    sys.exit("ERROR: se publico el ejecutable pese al rechazo")
print("Dos origenes de tls_read: diagnostico de ambiguedad correcto")
