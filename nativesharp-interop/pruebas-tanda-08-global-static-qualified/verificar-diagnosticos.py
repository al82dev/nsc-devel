import pathlib
import sys

log = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace")
for statement in ("limit = 1U", "fixedptr = null", "*readptr = 2"):
    if statement not in log:
        raise SystemExit(f"Falta diagnostico para {statement}")
if log.count("error:") < 3:
    raise SystemExit("Se requieren tres errores independientes")
print("Tres escrituras const rechazadas")
