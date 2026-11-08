from pathlib import Path
import shutil
import subprocess
import sys

root = Path(r"A:\nsc-interop-build\t08-tls-dos-dll-repro")
source = Path(__file__).resolve().parents[2] / "pruebas-tanda-08-tls-c" / "directo" / "ProbePeTls.c"
work = root / "control-import-alias"
relocated = work / "trasladado"
relocated.mkdir(parents=True, exist_ok=True)
alias = "alias-importado.dll"
probe = source.read_text(encoding="utf-8")
if probe.count('GetModuleHandleA("api.dll")') != 1:
    sys.exit("ERROR: cambio inesperado en el control TLS")
(work / "probe.c").write_text(probe.replace('GetModuleHandleA("api.dll")',
    f'GetModuleHandleA("{alias}")'), encoding="utf-8")
(work / "alias.def").write_text("LIBRARY " + alias + "\nEXPORTS\n tls_read\n tls_exported DATA\n", encoding="ascii")
shutil.copyfile(root / "uno" / "api.dll", relocated / alias)
commands = [
    ["llvm-dlltool.exe", "-m", "i386:x86-64", "-d", "alias.def", "-l", "alias.lib"],
    ["clang.exe", "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "probe.c", "-o", "probe.obj"],
    ["lld-link.exe", "/nologo", "/entry:mainCRTStartup", "/subsystem:console", "/nodefaultlib",
     "/out:probe.exe", "probe.obj", "alias.lib",
     r"A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib"],
]
for command in commands:
    result = subprocess.run(command, cwd=work, capture_output=True, text=True, errors="replace")
    if result.returncode:
        print(result.stdout + result.stderr)
        sys.exit(result.returncode)
imports = subprocess.run(["llvm-readobj.exe", "--coff-imports", str(work / "probe.exe")],
                         capture_output=True, text=True, errors="replace")
if imports.returncode or alias not in imports.stdout or "api.dll" in imports.stdout.lower():
    print(imports.stdout + imports.stderr)
    sys.exit("ERROR: la tabla de importaciones no apunta exclusivamente al alias")
shutil.copyfile(work / "probe.exe", relocated / "probe.exe")
result = subprocess.run([str(relocated / "probe.exe")], cwd=Path("A:/"))
if result.returncode:
    sys.exit(f"ERROR: el control TLS trasladado fallo con codigo {result.returncode}")
print("Importacion directa por alias: TLS compartido en dos hilos tras traslado: correcto")
