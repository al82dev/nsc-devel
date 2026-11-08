from pathlib import Path
import os
import shutil
import subprocess
import sys
import uuid

fixtures = Path(__file__).resolve().parent
root = Path(r"A:\nsc-interop-build\t08-tls-dos-dll-repro")
work = root / "directo-precompilado"
work.mkdir(parents=True, exist_ok=True)
for name in ("api.dll", "api.lib"):
    (work / name).unlink(missing_ok=True)
for name in ("ProducerDirecto.ns", "ProducerDirecto.nsproj", "ConsumerDirectoPrecompilado.ns",
             "ConsumerDirectoPrecompilado.nsproj"):
    shutil.copyfile(fixtures / name, work / name)
shutil.copyfile(fixtures.parents[1] / "pruebas-tanda-08-tls-c" / "directo" / "TlsApi.h", work / "TlsApi.h")
env = os.environ.copy()
env["PATH"] = r"A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;" + env["PATH"]
env["NSC_LINKER"] = r"A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
env["NSC_LLVM_LIB"] = r"A:\al82dev-clang\bin\llvm-lib.exe"
env["NSC_WINDOWS_RUNTIME_LIBRARY"] = r"A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
opt = env.get("NSC_T08_OPT", "0")
base = [r"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe", "--cpp-clang-root=A:\\al82dev-clang"]
producer = base + ["--emit-project-oop-lib", "ProducerDirecto.nsproj", "-O" + opt,
                   "--no-artifact-cache", "-o", "TlsProducer.lib"]
cache = Path(r"A:\t8-cache") / uuid.uuid4().hex[:12]
consumer = base + ["--emit-project-oop-exe", "ConsumerDirectoPrecompilado.nsproj", "-O" + opt,
                   "--time", "--artifact-cache=" + str(cache), "-o", "salida/Consumer.exe"]
def run(command, cwd=work):
    result = subprocess.run(command, cwd=cwd, env=env, capture_output=True, text=True, errors="replace")
    if result.returncode:
        print(result.stdout + result.stderr)
        sys.exit(result.returncode)
    return result.stdout + result.stderr

run(producer)
output = work / "salida"
output.mkdir(exist_ok=True)
for stale in output.glob("__nsc_tls_*.dll"):
    stale.unlink()
if (output / "api.dll").exists():
    sys.exit("ERROR: el destino contiene api.dll")
for attempt in range(2):
    log = run(consumer)
    (work / f"cache-{attempt + 1}.log").write_text(log, encoding="utf-8")
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
print("Funcion C precompilada con TLS y restauracion desde cache: correcto")
