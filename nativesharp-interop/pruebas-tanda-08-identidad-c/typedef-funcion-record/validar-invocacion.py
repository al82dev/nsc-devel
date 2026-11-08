from pathlib import Path
import subprocess
import sys

base = Path(__file__).parent.parent / "tls-dll-distintas" / "validar-variadicas.py"
source = base.read_text(encoding="utf-8")
changes = (
    (r"A:\nsc-interop-build\t08-tls-variadicas", r"A:\nsc-interop-build\t08-typedef-record-invocacion"),
    ('"extern __declspec(thread) int tls_exported;\\n"',
     '"typedef struct Payload { int value; } Payload;\\ntypedef int (*VarFn)(Payload*, int, ...);\\n__declspec(dllimport) Payload* get_payload(void);\\nextern __declspec(thread) int tls_exported;\\n"'),
    ('"__declspec(dllimport) int var_read(int count, ...);\\n"',
     '"__declspec(dllimport) int var_read(Payload*, int count, ...);\\n"'),
    ('__declspec(thread) int tls_exported = {initial};',
     'typedef struct Payload {{ int value; }} Payload;\nstatic Payload payload = {{{index}0}};\n__declspec(dllexport) Payload* get_payload(void) {{ return &payload; }}\n__declspec(thread) int tls_exported = {initial};'),
    ('int var_read(int count, ...) {{', 'int var_read(Payload* p, int count, ...) {{'),
    ('int result = GetCurrentThreadId() ? tls_exported : -1000;',
     'int result = GetCurrentThreadId() ? tls_exported + p->value : -1000;'),
    ('return var_read(6,', 'VarFn fn = var_read; return fn(get_payload(), 6,'),
    ('"__declspec(dllimport) int var_read(int, ...);\\n"',
     '"typedef struct Payload { int value; } Payload;\\n__declspec(dllimport) int var_read(Payload*, int, ...);\\n"'),
    ('expected = initial + sum(index * 10 + i for i in range(1, 7))',
     'expected = initial + index * 10 + sum(index * 10 + i for i in range(1, 7))'),
    ('void mainCRTStartup(void) {{ ExitProcess(var_read(6,',
     'void mainCRTStartup(void) {{ Payload p = {{{index}0}}; ExitProcess(var_read(&p, 6,'),
    ('!= 122 || Variadic2.Bridge.Read<int>(0) != 192',
     '!= 132 || Variadic2.Bridge.Read<int>(0) != 212'),
    ('(root / "Consumer.ns").write_text',
     'for index in (1, 2):\n    (root / str(index) / "api.h").unlink()\n\n(root / "Consumer.ns").write_text'),
    ('Dos funciones variadicas homonimas con TLS y cache:',
     'Invocacion con record C y dos DLL con cache:'),
)
for old, new in changes:
    if old not in source:
        sys.exit("ERROR: el control base cambio: " + old)
    source = source.replace(old, new)
work = Path(r"A:\nsc-interop-build\t08-typedef-record-invocacion")
if "--nested" in sys.argv:
    nested = (
        ('typedef struct Payload { int value; } Payload;',
         'typedef struct Inner { int value; } Inner; typedef struct Payload { Inner inner; } Payload;'),
        ('typedef struct Payload {{ int value; }} Payload;',
         'typedef struct Inner {{ int value; }} Inner; typedef struct Payload {{ Inner inner; }} Payload;'),
        ('Payload payload = {{{index}0}};', 'Payload payload = {{{{{index}0}}}};'),
        ('Payload p = {{{index}0}};', 'Payload p = {{{{{index}0}}}};'),
        ('p->value', 'p->inner.value'),
        (r"A:\nsc-interop-build\t08-typedef-record-invocacion",
         r"A:\nsc-interop-build\t08-typedef-record-anidado"),
    )
    for old, new in nested:
        if old not in source:
            sys.exit("ERROR: la variante no puede localizar: " + old)
        source = source.replace(old, new)
    work = Path(r"A:\nsc-interop-build\t08-typedef-record-anidado")
work.mkdir(parents=True, exist_ok=True)
script = work / "control-generado.py"
script.write_text(source, encoding="utf-8")
args = [arg for arg in sys.argv[1:] if arg != "--nested"]
sys.exit(subprocess.run([sys.executable, str(script), *args]).returncode)
