from pathlib import Path
import subprocess
import sys

base = Path(__file__).parent.parent / "tls-dll-distintas" / "validar-variadicas.py"
source = base.read_text(encoding="utf-8")
large = "--large" in sys.argv
fields = "int value; long long padding1; long long padding2;" if large else "int value;"
changes = (
    (r"A:\nsc-interop-build\t08-tls-variadicas",
     r"A:\nsc-interop-build\t08-typedef-record-retorno-grande" if large else
     r"A:\nsc-interop-build\t08-typedef-record-retorno-pequeno"),
    ('"extern __declspec(thread) int tls_exported;\\n"',
     f'"typedef struct Payload {{ {fields} }} Payload;\\ntypedef Payload (*VarFn)(int, ...);\\nextern __declspec(thread) int tls_exported;\\n"'),
    ('"__declspec(dllimport) int var_read(int count, ...);\\n"',
     '"__declspec(dllimport) Payload var_read(int count, ...);\\n"'),
    ('__declspec(thread) int tls_exported = {initial};',
     f'typedef struct Payload {{{{ {fields} }}}} Payload;\n__declspec(thread) int tls_exported = {{initial}};'),
    ('int var_read(int count, ...) {{', 'Payload var_read(int count, ...) {{'),
    ('    return result;\n}}\n__declspec(dllexport) int var_mix',
     '    Payload output = {{0}}; output.value = result; return output;\n}}\n__declspec(dllexport) int var_mix'),
    ('return var_read(6,', 'VarFn fn = var_read; Payload result = fn(6,'),
    ('{index * 10 + 6}); }}', '{index * 10 + 6}); return result.value; }}'),
    ('"__declspec(dllimport) int var_read(int, ...);\\n"',
     f'"typedef struct Payload {{ {fields} }} Payload;\\n__declspec(dllimport) Payload var_read(int, ...);\\n"'),
    ('var_read(6, {values}) != {expected}', 'var_read(6, {values}).value != {expected}'),
    ('(root / "Consumer.ns").write_text',
     'for index in (1, 2):\n    (root / str(index) / "api.h").unlink()\n\n(root / "Consumer.ns").write_text'),
    ('Dos funciones variadicas homonimas con TLS y cache:',
     'Retorno de record C por puntero a funcion con cache:'),
)
for old, new in changes:
    if old not in source:
        sys.exit("ERROR: el control base cambio: " + old)
    source = source.replace(old, new)
if large:
    extra = (
        ('output.value = result; return output;',
         'output.value = result; output.padding1 = 123456789; output.padding2 = 987654321; return output;'),
        ('return result.value; }}',
         'if (result.padding1 != 123456789 || result.padding2 != 987654321) return -1; return result.value; }}'),
        ('var_read(6, {values}).value != {expected}',
         'var_read(6, {values}).value != {expected} || var_read(6, {values}).padding1 != 123456789 || var_read(6, {values}).padding2 != 987654321'),
    )
    for old, new in extra:
        if old not in source:
            sys.exit("ERROR: el control grande no puede localizar: " + old)
        source = source.replace(old, new)
work = Path(r"A:\nsc-interop-build\t08-typedef-record-retorno-grande" if large else
            r"A:\nsc-interop-build\t08-typedef-record-retorno-pequeno")
work.mkdir(parents=True, exist_ok=True)
script = work / "control-generado.py"
script.write_text(source, encoding="utf-8")
args = [arg for arg in sys.argv[1:] if arg != "--large"]
sys.exit(subprocess.run([sys.executable, str(script), *args]).returncode)
