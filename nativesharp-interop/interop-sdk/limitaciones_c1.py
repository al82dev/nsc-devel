"""Limitaciones C1: calificadores por nivel de puntero en C (plan de limitaciones, tareas C1-01..C1-06).

NativeSharp conserva cada nivel de una cadena de punteros: la base con las marcas del apuntado (`T const**`,
`T volatile**`) y cada puntero intermedio con su propio `const`/`volatile` (`int* volatile*`, `char const* const*`).

Positivos (O0-O3; directo, productor .lib y productor DLL con consumidor separado):
  - parametros `volatile int**`, `const volatile int**`, `int* volatile*`, `int* const*`; retorno `volatile int**`;
  - campo C `volatile int** pp` y `int* volatile* q`; global C `volatile int** g` y `int* volatile g_ptr`;
  - callback C cuyo parametro es `volatile int**`;
  - `&p` de un puntero a dato volatile (`int volatile**`) y de una global `int* volatile` (`int* volatile*`).
Rechazos (O0): agregar un calificador en un nivel profundo, perderlo en cualquier nivel.
IR (O1): `**q` y la lectura por un nivel intermedio volatile son cargas volatile; `*q` de un nivel sin calificar no.
"""
from comun import Caso, positivo, rechazo, ir, fin, _fail, ONLY

H = r'''
int bump(volatile int** pp);
int read_cv(const volatile int** pp);
volatile int** cell(void);
int mid(int* volatile* pp);
int readc(int* const* pp);
typedef struct Holder { volatile int** pp; int* volatile* q; int tag; } Holder;
Holder* holder(void);
extern volatile int** g_cell;
extern int* volatile g_ptr;
typedef int (*vcb_t)(volatile int** pp);
int with_vcb(vcb_t cb);
'''
S = r'''
static int value = 1;
static volatile int* slot = &value;
static int other = 3;
static int* volatile vslot = &other;
volatile int** g_cell = &slot;
int* volatile g_ptr = &other;
static Holder h = {&slot, &vslot, 7};
int bump(volatile int** pp) { **pp += 1; return **pp; }
int read_cv(const volatile int** pp) { return **pp; }
volatile int** cell(void) { return &slot; }
int mid(int* volatile* pp) { return **pp; }
int readc(int* const* pp) { return **pp; }
Holder* holder(void) { return &h; }
int with_vcb(vcb_t cb) { return cb(&slot); }
'''
PRELUDIO = '''
public static class Cb
{
    public static int Twice(int volatile** q) { **q = **q * 2; return **q; }
}
'''
BODY = r'''
    int x = 10;
    int volatile* px = &x;
    int volatile** ppx = &px;
    if (bump(ppx) != 11 || x != 11) return 1;
    int const volatile* cx = &x;
    int const volatile** pcx = &cx;
    if (read_cv(pcx) != 11) return 2;
    int volatile** c = cell();
    **c = 5;
    if (**c != 5) return 3;
    Holder* h = holder();
    **h->pp = 6;
    if (**h->pp != 6 || **h->q != 3 || h->tag != 7) return 4;
    **g_cell = 20;
    if (**g_cell != 20) return 5;
    if (with_vcb(Cb.Twice) != 40) return 6;
    int y = 2; int* py = &y;
    int* volatile* vpy = &py;
    if (mid(vpy) != 2 || readc(&py) != 2) return 7;
    int* volatile* gp = &g_ptr;
    if (mid(gp) != 3 || mid(h->q) != 3) return 8;
    int volatile* reseat = *c;
    if (*reseat != 40) return 9;
    return 42;
'''
c = Caso('c1-niveles', H, S)
positivo(c, BODY, PRELUDIO)
# C: only the object the top pointer points to may gain a qualifier; no level may lose one.
rechazo(c, 'int z = 1; int* pz = &z; int** ppz = &pz; return bump(ppz);', 'bump')
rechazo(c, 'int z = 1; int volatile* pz = &z; int volatile** a = &pz; int** b = a; return 0;', 'volatile')
rechazo(c, 'int z = 1; int* pz = &z; int volatile** a = &pz; return 0;', "'T volatile**' implicitly")
rechazo(c, 'int z = 1; int* pz = &z; int* volatile* a = &pz; int** b = a; return 0;',
        'would discard a qualifier of a pointer level')
rechazo(c, 'int z = 1; int* pz = &z; int** a = &pz; int*** b = &a; int* volatile** d = b; return 0;',
        'only the object the pointer points to may gain a qualifier implicitly')

# C1-05: a NativeSharp library publishes `int* volatile*` and `int volatile**`; a separate consumer sees every level
# through the metadata only (no header, no library sources).
LIB = '''using NativeSharp.Core;
namespace Niveles
{
    public static class Api
    {
        public static int Read(int* volatile* p) { return **p; }
        public static int* volatile* Pass(int* volatile* p) { return p; }
        public static int Base(int volatile** p) { return **p; }
    }
}
'''
CONSUMER_OK = '''using NativeSharp.Core;
int Main()
{
    int z = 20; int* pz = &z;
    int* volatile* v = &pz;
    int volatile* w = &z;
    int volatile** pw = &w;
    return Niveles.Api.Read(v) + Niveles.Api.Read(&pz) + Niveles.Api.Base(pw) - 18 + (Niveles.Api.Pass(v) == v ? 0 : 1);
}
'''
CONSUMER_BAD = '''using NativeSharp.Core;
int Main()
{
    int z = 1; int* pz = &z;
    int** b = Niveles.Api.Pass(&pz);
    return 0;
}
'''
if not ONLY or any('c1-meta' in o for o in ONLY):
    from comun import ROOT, project, nsc, core, _execute
    for opt in ('0', '2'):
        for kind, output_type, artifact in (('lib', 'Library', 'Niveles.lib'), ('dll', 'SharedLibrary', 'Niveles.dll')):
            label = f'c1-meta-O{opt}-{kind}'
            base = ROOT / 'c1-meta' / f'O{opt}' / kind
            producer = base / 'productor'
            producer.mkdir(parents=True)
            (producer / 'Lib.ns').write_text(LIB, encoding='utf-8')
            project(producer, 'Niveles', ['Lib.ns'], core(opt), output_type)
            built = nsc(producer, 'Niveles', opt, kind, artifact, 'nsc')
            if built.returncode:
                _fail(label + '-productor', built.stdout[-1500:])
                continue
            runtime = ' Runtime="../productor/Niveles.dll"' if kind == 'dll' else ''
            for name, source, expect_ok in (('consumidor', CONSUMER_OK, True), ('rechazo', CONSUMER_BAD, False)):
                consumer = base / name
                consumer.mkdir()
                (consumer / 'Main.ns').write_text(source, encoding='utf-8')
                project(consumer, 'App', ['Main.ns'],
                        f'<LibraryReference Include="../productor/Niveles.lib"{runtime} />' + core(opt))
                result = nsc(consumer, 'App', opt, 'exe', 'app.exe', 'nsc')
                if expect_ok:
                    if result.returncode:
                        _fail(f'{label}-{name}', result.stdout[-1500:])
                        continue
                    if kind == 'dll':
                        (consumer / 'Niveles.dll').write_bytes((producer / 'Niveles.dll').read_bytes())
                    if _execute(f'{label}-{name}', consumer / 'app.exe'):
                        print(f'ok {label}-{name}', flush=True)
                elif result.returncode == 0 or 'would discard a qualifier of a pointer level' not in result.stdout:
                    _fail(f'{label}-{name}', 'se esperaba el rechazo por nivel:\n' + result.stdout[-1500:])
                else:
                    print(f'ok {label}-{name}', flush=True)

if not ONLY or any('c1-ir' in o for o in ONLY):
    caso_ir = Caso('c1-ir', 'int sink(int v);\n', 'int sink(int v) { return v; }\n')
    result = ir(caso_ir, 'int z = 3; int volatile* p = &z; int volatile** q = &p; int volatile* r = *q; '
                         'int* w = &z; int* volatile* v = &w; int* s = *v; '
                         'return sink(**q) + sink(*r) + sink(*s);')
    text = result.stdout
    volatile_int = text.count('load volatile i32')
    volatile_ptr = text.count('load volatile ptr')
    if result.returncode or volatile_int < 2 or volatile_ptr < 1:
        _fail('c1-ir', f'cargas volatile i32={volatile_int} ptr={volatile_ptr} (se esperaban >=2 y >=1):\n'
                       f'{text[-3000:]}')
    else:
        print(f'ok c1-ir (volatile i32={volatile_int}, ptr={volatile_ptr})', flush=True)
fin()
