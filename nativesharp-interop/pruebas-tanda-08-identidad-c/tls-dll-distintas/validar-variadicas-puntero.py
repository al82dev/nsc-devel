from pathlib import Path
import subprocess
import sys

source = Path(__file__).with_name("validar-variadicas.py").read_text(encoding="utf-8")
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
work = Path(r"A:\nsc-interop-build\t08-tls-variadicas-puntero")
work.mkdir(parents=True, exist_ok=True)
script = work / "control-generado.py"
script.write_text(source, encoding="utf-8")
sys.exit(subprocess.run([sys.executable, str(script), *sys.argv[1:]]).returncode)
