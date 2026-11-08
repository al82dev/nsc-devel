from pathlib import Path
import subprocess
import sys
import tempfile

original = Path(__file__).resolve().parent.parent / "typedef-funcion-record" / "validar-invocacion.py"
base = original.parent.parent / "tls-dll-distintas" / "validar-variadicas.py"
opt = sys.argv[1] if len(sys.argv) > 1 else "0"
qualifier = sys.argv[2] if len(sys.argv) > 2 else "volatile"
if qualifier not in ("volatile", "const volatile"):
    raise SystemExit("calificador desconocido")
root = Path(tempfile.mkdtemp(prefix="t08-callback-invoke-", dir=r"A:\nsc-interop-build"))
source = original.read_text(encoding="utf-8")
source = source.replace('base = Path(__file__).parent.parent / "tls-dll-distintas" / "validar-variadicas.py"',
                        "base = Path(" + repr(str(base)) + ")")
source = source.replace(r"A:\nsc-interop-build\t08-typedef-record-invocacion", str(root))
insert = "source = source.replace('Payload*', " + repr(qualifier + " Payload*") + ")\n"
insert += "source = source.replace('static Payload payload', " + repr("static " + qualifier + " Payload payload") + ")\n"
source = source.replace("script.write_text(source, encoding=\"utf-8\")", insert +
                        "script.write_text(source, encoding=\"utf-8\")")
generated = root / "wrapper.py"
generated.write_text(source, encoding="utf-8")
result = subprocess.run([sys.executable, str(generated), opt], cwd=root, text=True,
                        encoding="utf-8", errors="replace", stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT)
if result.returncode == 0 or "indirect call cannot yet preserve pointer pointee volatile qualification" not in result.stdout:
    print(result.stdout)
    raise SystemExit("La reproducción dejó de coincidir con el diagnóstico esperado")
print("Reproducido rechazo de callback", qualifier, "en O" + opt, ";", root)
