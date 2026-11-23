"""Limitaciones R1: contratos de frontera (plan de limitaciones, tareas R1-01..R1-05; decision D6).

Un `<Contract>` junto a la referencia del proyecto declara lo que el encabezado no dice
(implementacion-plan/CONTRATOS-PROPUESTA.md). Sin contrato no se agrega ninguna comprobacion (R1-01):
- `Kind="Retained" Until="Static"`: la direccion de un local o memoria de pila se rechaza; un puntero que no es de
  un local (memoria de la biblioteca) se acepta.
- `Until="Call:<f>"`: se acepta un local cuyo bloque llama a f despues (registro y desregistro, R1-03); sin esa
  llamada, o desde un bloque interno que termina antes, se rechaza.
- `Kind="Transfer"`: la biblioteca toma la propiedad; un local o memoria de pila se rechaza.
- `Result="Owned" Release="f"`: descartar el resultado se rechaza (se libera con f o se conserva).
- `Kind="NonNull"`, `"Nullable"`, `"Out" Optional="true"`: nulidad del parametro, como 8.3.
- Un contrato que nombra una funcion que no se importa es un error del proyecto.
Persistir contratos en la metadata precompilada (R1-05) necesita acordar su representacion en NSMeta: no se hace;
los contratos aplican a la importacion directa del proyecto que los declara.
"""
import comun
from comun import Caso, positivo, rechazo, fin

H = r'''
typedef void (*tick_fn)(void* user);
int add_timer(tick_fn cb, void* user);
void remove_timer(int id);
int fire_timers(void);
void keep_forever(int* p);
int* global_slot(void);
void take_buffer(char* p);
char* make_buffer(int n);
void free_buffer(char* p);
int get_pos(int* x, int* y);
int need_ptr(int* p);
'''
S = r'''
static tick_fn cbs[4]; static void* users[4];
int add_timer(tick_fn cb, void* user) { for (int i = 0; i < 4; ++i) if (!cbs[i]) { cbs[i] = cb; users[i] = user; return i; } return -1; }
void remove_timer(int id) { cbs[id] = 0; users[id] = 0; }
int fire_timers(void) { int n = 0; for (int i = 0; i < 4; ++i) if (cbs[i]) { cbs[i](users[i]); ++n; } return n; }
static int* kept; static int slot = 40;
void keep_forever(int* p) { kept = p; }
int* global_slot(void) { return &slot; }
static char pool[64]; static int used;
void take_buffer(char* p) { (void)p; }
char* make_buffer(int n) { char* p = pool + used; used += n; return p; }
void free_buffer(char* p) { (void)p; }
int get_pos(int* x, int* y) { *x = 40; if (y) *y = 2; return 0; }
int need_ptr(int* p) { return p ? *p : -1; }
'''
CONTRACTS = r'''
  <Contract Function="add_timer" Parameter="1" Kind="Retained" Until="Call:remove_timer" />
  <Contract Function="keep_forever" Parameter="0" Kind="Retained" Until="Static" />
  <Contract Function="take_buffer" Parameter="0" Kind="Transfer" />
  <Contract Function="make_buffer" Result="Owned" Release="free_buffer" />
  <Contract Function="get_pos" Parameter="0" Kind="Out" />
  <Contract Function="get_pos" Parameter="1" Kind="Out" Optional="true" />
  <Contract Function="need_ptr" Parameter="0" Kind="NonNull" />
'''


class CasoContratos(Caso):
    """A reference with `<Contract>` children. / Una referencia con hijos `<Contract>`."""

    def __init__(self, nombre, header, source, contracts, **kwargs):
        super().__init__(nombre, header, source, **kwargs)
        self.contracts = contracts

    def reference(self, folder):
        text = super().reference(folder)
        assert text.endswith(' />')
        return text[:-3] + '>' + self.contracts + '</CppLibraryReference>'


PRE = 'struct Counter { public int hits; }\n'
BODY = r'''
    Counter c; c.hits = 0;
    int id = add_timer((u) => { Counter* p = (Counter*)u; p->hits++; }, &c);
    fire_timers(); fire_timers();
    remove_timer(id);
    if (c.hits != 2) return 1;
    keep_forever(global_slot());
    take_buffer(make_buffer(4));
    var b = make_buffer(8); b[0] = 1; free_buffer(b);
    int x = 0;
    if (get_pos(&x, null) != 0 || x != 40) return 2;
    int y = 0;
    get_pos(&x, &y);
    return x + y + need_ptr(&y) - 2;
'''
c = CasoContratos('r1-contratos', H, S, CONTRACTS)
positivo(c, BODY, preludio=PRE, modes=['directo'])
rechazo(c, 'int s = 0; keep_forever(&s); return 0;', 'retains argument 0 until Static', preludio=PRE)
rechazo(c, 'Counter c; c.hits = 0; add_timer((u) => { }, &c); return 0;', 'until Call:remove_timer', preludio=PRE)
rechazo(c, 'int id = 0; { Counter c; c.hits = 0; id = add_timer((u) => { }, &c); } remove_timer(id); return 0;',
        'until Call:remove_timer', preludio=PRE)
rechazo(c, 'sbyte* sb = stackalloc sbyte[8]; take_buffer(sb); return 0;', 'takes ownership of argument 0', preludio=PRE)
rechazo(c, 'make_buffer(4); return 0;', 'discarding it leaks it', preludio=PRE)
rechazo(c, 'return need_ptr(null);', 'null', preludio=PRE)
# R1-04: an owned result kept in a local is released, transferred, returned or stored on every path.
# Un resultado con dueno guardado en un local se libera, transfiere, devuelve o guarda en todo camino.
PRE4 = PRE + ('sbyte* Keep(int n) { var b = make_buffer(n); return b; }\n'
              'class G { public static sbyte* stored; }\n'
              'void Store() { var b = make_buffer(2); G.stored = b; }\n')
BODY4 = r'''
    int k = 3;
    var a = make_buffer(4);
    if (k > 2) { a[0] = 1; free_buffer(a); } else free_buffer(a);
    var t = make_buffer(4); take_buffer(t);
    var w = make_buffer(4);
    if (k == 0) { free_buffer(w); return 1; }
    for (int i = 0; i < 2; ++i) { k++; }
    try { w[0] = 2; free_buffer(w); } catch { free_buffer(w); }
    var r = Keep(1); free_buffer(r);
    Store(); free_buffer(G.stored);
    return 42;
'''
positivo(CasoContratos('r1-liberacion', H, S, CONTRACTS), BODY4, preludio=PRE4, modes=['directo'])
rechazo(c, 'int k = 3; var a = make_buffer(4); if (k > 2) free_buffer(a); return 0;',
        "is not released before this return", preludio=PRE)
rechazo(c, 'int k = 3; var a = make_buffer(4); if (k > 2) return 1; free_buffer(a); return 0;',
        "is not released before this return", preludio=PRE)
rechazo(c, 'int k = 0; { var a = make_buffer(4); while (k < 2) { k++; free_buffer(a); } } return k;',
        "is not released before the end of its block", preludio=PRE)
rechazo(CasoContratos('r1-contrato-desconocido', H, S, '<Contract Function="nope" Parameter="0" Kind="NonNull" />'),
        'return 0;', "contract names 'nope'")
fin()
