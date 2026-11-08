from pathlib import Path
import subprocess
import sys

source = Path(__file__).with_name("validar-variadicas.py").read_text(encoding="utf-8")
same_alias = "--same-alias" in sys.argv or "--mismatch" in sys.argv
mismatch = "--mismatch" in sys.argv
changes = (
    (r"A:\nsc-interop-build\t08-tls-variadicas", r"A:\nsc-interop-build\t08-tls-variadicas-puntero"),
    ('"__declspec(dllimport) int var_read(int count, ...);\\n"',
     'f"typedef int (*VarFn{index})(int, ...);\\n" "__declspec(dllimport) int var_read(int count, ...);\\n"'),
    ('return var_read(6,', 'VarFn{index} fn = var_read; return fn(6,'),
    ('Dos funciones variadicas homonimas con TLS y cache:',
     'Dos punteros a funciones variadicas con TLS y cache:'),
)
for old, new in changes:
    if old not in source:
        sys.exit("ERROR: el control base cambio; no se puede construir la variante")
    source = source.replace(old, new)
if same_alias:
    source = source.replace("VarFn{index}", "VarFn")
if mismatch:
    source = source.replace("typedef int (*VarFn)(int, ...)",
                            "typedef int (*VarFn)({'int' if index == 1 else 'long long'}, ...)")
work = Path(r"A:\nsc-interop-build\t08-tls-variadicas-puntero")
work.mkdir(parents=True, exist_ok=True)
script = work / "control-generado.py"
script.write_text(source, encoding="utf-8")
args = [arg for arg in sys.argv[1:] if arg not in ("--same-alias", "--mismatch")]
result = subprocess.run([sys.executable, str(script), *args], capture_output=True, text=True, errors="replace")
if mismatch:
    if result.returncode == 0 or "conflicting C/C++ type alias 'VarFn'" not in result.stdout + result.stderr:
        print(result.stdout + result.stderr)
        sys.exit("ERROR: se esperaba rechazar el typedef de firma incompatible")
    print("Typedefs de firma incompatible: rechazo correcto")
    sys.exit(0)
print(result.stdout + result.stderr, end="")
sys.exit(result.returncode)
