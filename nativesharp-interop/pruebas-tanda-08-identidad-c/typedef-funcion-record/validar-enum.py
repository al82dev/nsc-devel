from pathlib import Path
import subprocess
import sys

base = Path(__file__).parent.parent / "tls-dll-distintas" / "validar-variadicas.py"
source = base.read_text(encoding="utf-8")
changes = (
    (r"A:\nsc-interop-build\t08-tls-variadicas", r"A:\nsc-interop-build\t08-typedef-enum-invocacion"),
    ('"extern __declspec(thread) int tls_exported;\\n"',
     '"typedef enum CKind { KValue = 7 } CKind;\\ntypedef int (*VarFn)(CKind, int, ...);\\n__declspec(dllimport) CKind get_kind(void);\\nextern __declspec(thread) int tls_exported;\\n"'),
    ('"__declspec(dllimport) int var_read(int count, ...);\\n"',
     '"__declspec(dllimport) int var_read(CKind, int count, ...);\\n"'),
    ('__declspec(thread) int tls_exported = {initial};',
     'typedef enum CKind {{ KValue = 7 }} CKind;\n__declspec(dllexport) CKind get_kind(void) {{ return KValue; }}\n__declspec(thread) int tls_exported = {initial};'),
    ('int var_read(int count, ...) {{', 'int var_read(CKind kind, int count, ...) {{'),
    ('int result = GetCurrentThreadId() ? tls_exported : -1000;',
     'int result = GetCurrentThreadId() ? tls_exported + (int)kind : -1000;'),
    ('return var_read(6,', 'VarFn fn = var_read; return fn(KValue, 6,'),
    ('"__declspec(dllimport) int var_read(int, ...);\\n"',
     '"typedef enum CKind { KValue = 7 } CKind;\\n__declspec(dllimport) int var_read(CKind, int, ...);\\n"'),
    ('expected = initial + sum(index * 10 + i for i in range(1, 7))',
     'expected = initial + 7 + sum(index * 10 + i for i in range(1, 7))'),
    ('ExitProcess(var_read(6,', 'ExitProcess(var_read((CKind)7, 6,'),
    ('!= 122 || Variadic2.Bridge.Read<int>(0) != 192',
     '!= 129 || Variadic2.Bridge.Read<int>(0) != 199'),
    ('(root / "Consumer.ns").write_text',
     'for index in (1, 2):\n    (root / str(index) / "api.h").unlink()\n\n(root / "Consumer.ns").write_text'),
    ('Dos funciones variadicas homonimas con TLS y cache:',
     'Invocacion con enum C y dos DLL con cache:'),
)
for old, new in changes:
    if old not in source:
        sys.exit("ERROR: el control base cambio: " + old)
    source = source.replace(old, new)
mismatch = "--mismatch" in sys.argv
ambiguous = "--consumer-ambiguous" in sys.argv
returns_enum = "--returns" in sys.argv
work = Path(r"A:\nsc-interop-build\t08-typedef-enum-invocacion")
if returns_enum:
    variants = (
        ('typedef int (*VarFn)(CKind, int, ...);', 'typedef CKind (*VarFn)(CKind, int, ...);'),
        ('__declspec(dllimport) int var_read(CKind', '__declspec(dllimport) CKind var_read(CKind'),
        ('int var_read(CKind kind, int count, ...) {{', 'CKind var_read(CKind kind, int count, ...) {{'),
        ('    return result;\n}}\n__declspec(dllexport) int var_mix',
         '    return (CKind)result;\n}}\n__declspec(dllexport) int var_mix'),
        ('return fn(KValue, 6,', 'return (int)fn(KValue, 6,'),
        ('Invocacion con enum C y dos DLL con cache:', 'Retorno enum C y dos DLL con cache:'),
    )
    for old, new in variants:
        if old not in source:
            sys.exit("ERROR: no se puede preparar el retorno enum: " + old)
        source = source.replace(old, new)
    source = source.replace(str(work), str(work) + "-retorno")
    work = Path(str(work) + "-retorno")
if mismatch:
    old = '"typedef enum CKind { KValue = 7 } CKind;\\n'
    new = 'f"typedef enum CKind {{ KValue = 7, KOther = {index} }} CKind;\\n'
    if old not in source or 'typedef enum CKind {{ KValue = 7 }} CKind;' not in source:
        sys.exit("ERROR: no se puede preparar el enum incompatible")
    source = source.replace(old, new)
    source = source.replace('typedef enum CKind {{ KValue = 7 }} CKind;',
                            'typedef enum CKind {{ KValue = 7, KOther = {index} }} CKind;')
    source = source.replace(str(work), str(work) + "-incompatible")
    work = Path(str(work) + "-incompatible")
if ambiguous:
    old = 'if (Variadic1.Bridge.Access<int>(0, 41) != 41 || Variadic2.Bridge.Access<int>(0, 51) != 51) return 1;'
    if old not in source:
        sys.exit("ERROR: no se puede preparar la referencia ambigua")
    source = source.replace(old, 'if (KValue != 7) return 1;')
    source = source.replace(str(work), str(work) + "-ambiguo")
    work = Path(str(work) + "-ambiguo")
work.mkdir(parents=True, exist_ok=True)
script = work / "control-generado.py"
script.write_text(source, encoding="utf-8")
args = [arg for arg in sys.argv[1:] if arg not in ("--mismatch", "--consumer-ambiguous", "--returns")]
result = subprocess.run([sys.executable, str(script), *args], capture_output=True, text=True, errors="replace")
if mismatch:
    # T08-D10: each precompiled producer resolves its own `VarFn` and `CKind` from its catalogue, so a consumer
    # composes two producers whose enums differ, as with the incompatible typedef variants of validar.py.
    # T08-D10: cada productor precompilado resuelve su `VarFn` y su `CKind` desde su catalogo.
    if result.returncode != 0:
        print(result.stdout + result.stderr)
        sys.exit("ERROR: el consumidor de dos productores con enums incompatibles fallo")
    print("Enums con valores incompatibles en productores distintos: consumidor correcto (T08-D10)")
    sys.exit(0)
if ambiguous:
    if result.returncode == 0 or "ambiguous namespace state reference 'KValue'" not in result.stdout + result.stderr:
        print(result.stdout + result.stderr)
        sys.exit("ERROR: se esperaba diagnosticar la referencia ambigua")
    print("Enumerador sin productor identificado: ambiguedad correcta")
    sys.exit(0)
print(result.stdout + result.stderr, end="")
sys.exit(result.returncode)
