from pathlib import Path
import os
import shutil
import subprocess
import sys
import uuid

fixtures = Path(__file__).resolve().parent
root = Path(r"A:\nsc-interop-build\t08-tls-dos-dll-repro")
work = root / "origen-ambiguo"
for name in ("Producer2Directo.ns", "Producer2Directo.nsproj", "ConsumerDosOrigenes.ns", "ConsumerDosOrigenes.nsproj",
             "ConsumerDosOrigenesHilos.ns", "ConsumerDosOrigenesHilos.nsproj", "ThreadOnly.h"):
    shutil.copyfile(fixtures / name, work / name)
env = os.environ.copy()
env["PATH"] = r"A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;" + env["PATH"]
env["NSC_LINKER"] = r"A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
env["NSC_LLVM_LIB"] = r"A:\al82dev-clang\bin\llvm-lib.exe"
env["NSC_WINDOWS_RUNTIME_LIBRARY"] = r"A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
nsc = r"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe"
base = [nsc, "--cpp-clang-root=A:\\al82dev-clang"]
opt = env.get("NSC_T08_OPT", "0")
def run(command, cwd=work):
    result = subprocess.run(command, cwd=cwd, env=env, capture_output=True, text=True, errors="replace")
    if result.returncode:
        print(result.stdout + result.stderr)
        print("ERROR: codigo", result.returncode, "al ejecutar", command[0])
        sys.exit(result.returncode)
    return result.stdout + result.stderr

run(base + ["--emit-project-oop-lib", "Producer2Directo.nsproj", "-O" + opt, "--no-artifact-cache", "-o", "TlsProducer2.lib"])
output = work / "dos"
output.mkdir(exist_ok=True)
for stale in output.glob("__nsc_tls_*.dll"):
    stale.unlink()
if (output / "api.dll").exists():
    sys.exit("ERROR: el destino contiene api.dll")
cache = Path(r"A:\t8-cache") / uuid.uuid4().hex[:12]
command = base + ["--emit-project-oop-exe", "ConsumerDosOrigenes.nsproj", "-O" + opt,
                  "--time", "--artifact-cache=" + str(cache), "-o", "dos/Consumer.exe"]
for attempt in range(2):
    log = run(command)
    marker = "compilation-bundle-cache: not-found" if attempt == 0 else "compilation-bundle-cache: hit"
    if marker not in log:
        print(log)
        sys.exit("ERROR: estado inesperado de la cache")
    aliases = list(output.glob("__nsc_tls_*.dll"))
    if len(aliases) != 2:
        sys.exit("ERROR: faltan las dos DLL TLS")
    run([str(output / "Consumer.exe")], Path("A:/"))
    if attempt == 0:
        (output / "Consumer.exe").unlink()
        for alias in aliases:
            alias.unlink()
print("Dos funciones C homonimas desde plantillas precompiladas y cache: correcto")
threaded = work / "dos-hilos"
threaded.mkdir(exist_ok=True)
for stale in threaded.glob("__nsc_tls_*.dll"):
    stale.unlink()
(threaded / "thread-only.dll").unlink(missing_ok=True)
if (threaded / "api.dll").exists():
    sys.exit("ERROR: el destino de hilos contiene api.dll")
run(base + ["--emit-project-oop-exe", "ConsumerDosOrigenesHilos.nsproj", "-O" + opt,
            "--no-artifact-cache", "-o", "dos-hilos/Consumer.exe"])
if len(list(threaded.glob("__nsc_tls_*.dll"))) != 2:
    sys.exit("ERROR: faltan las DLL TLS en el control de hilos")
run([str(threaded / "Consumer.exe")], Path("A:/"))
print("Dos funciones C homonimas y TLS en dos hilos: correcto")
