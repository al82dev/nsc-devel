from pathlib import Path
import subprocess
import sys

source = Path(__file__).resolve().with_name("InspeccionarTls.c")
dynamic_source = source.with_name("InspeccionarTlsDinamico.c")
root = Path(r"A:\nsc-interop-build\t08-tls-dos-dll-repro")
control = Path(__file__).resolve().with_name("validar-origen-ambiguo.py")
clang = r"A:\al82dev-clang\bin\clang.exe"
lld = r"A:\al82dev-clang\bin\lld-link.exe"
readobj = r"A:\al82dev-clang\bin\llvm-readobj.exe"
sdk = r"A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib"
for name, options in (("origen-sin-import", ["--sin-import"]),
                      ("origen-sin-import-entrada", ["--sin-import", "--entrada"]),
                      ("origen-import-sin-llamar", ["--sin-import", "--import-sin-llamar"]),
                      ("origen-ambiguo", ["--solo-c"])):
    prepared = subprocess.run([sys.executable, str(control)] + options, capture_output=True, text=True, errors="replace")
    if prepared.returncode:
        print(prepared.stdout + prepared.stderr)
        sys.exit(prepared.returncode)
    work = root / name
    imports = subprocess.run([readobj, "--coff-imports", str(work / "api.dll")],
                             capture_output=True, text=True, errors="replace")
    should_import = name in ("origen-ambiguo", "origen-import-sin-llamar")
    if imports.returncode or (("GetCurrentThreadId" in imports.stdout) != should_import):
        print(imports.stdout + imports.stderr)
        sys.exit("ERROR: la tabla de importaciones no corresponde al control")
    for command in ([clang, "-c", "--target=x86_64-pc-windows-msvc", str(source), "-o", "inspect.obj"],
                    [lld, "/nologo", "/entry:mainCRTStartup", "/subsystem:console", "/nodefaultlib",
                     "/out:inspect.exe", "inspect.obj", "api.lib", sdk]):
        result = subprocess.run(command, cwd=work, capture_output=True, text=True, errors="replace")
        if result.returncode:
            print(result.stdout + result.stderr)
            sys.exit(result.returncode)
    result = subprocess.run([str(work / "inspect.exe")], cwd=work, capture_output=True, text=True, errors="replace")
    print(name + ":\n" + result.stdout, end="")
    if result.returncode:
        sys.exit(result.returncode)
    try:
        values = {key: int(value, 16) for key, value in
                  (line.split("=", 1) for line in result.stdout.splitlines())}
        if values["raw4"] != 7 or values["read"] != values["slot4"]:
            raise ValueError("TLS raw and slot disagree with the control")
        if (values["read"] == 7) != (name in ("origen-ambiguo", "origen-import-sin-llamar")):
            raise ValueError("TLS initial behavior changed")
        if name == "origen-sin-import-entrada" and values.get("attach") != 1:
            raise ValueError("DllMain was not observed exactly once")
    except (KeyError, ValueError) as error:
        sys.exit("ERROR: " + str(error))
    for command in ([clang, "-c", "--target=x86_64-pc-windows-msvc", str(dynamic_source), "-o", "dynamic.obj"],
                    [lld, "/nologo", "/entry:mainCRTStartup", "/subsystem:console", "/nodefaultlib",
                     "/out:dynamic.exe", "dynamic.obj", sdk]):
        result = subprocess.run(command, cwd=work, capture_output=True, text=True, errors="replace")
        if result.returncode:
            print(result.stdout + result.stderr)
            sys.exit(result.returncode)
    result = subprocess.run([str(work / "dynamic.exe")], cwd=Path("A:/"))
    print("dynamic=" + str(result.returncode))
    expected = 0 if name in ("origen-ambiguo", "origen-import-sin-llamar") else 106
    if result.returncode != expected:
        sys.exit("ERROR: cambio el resultado de la carga dinamica: " + str(result.returncode))
