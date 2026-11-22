"""Tanda 15: subobjetos y layout C++ (items 15.1-15.7).

15.1 campos record, arreglo y escalar de un record C++ con constructor y destructor propios, leidos por su direccion;
15.2 arreglo de records en un campo C++ (`BlendDesc::targets`); 15.6 helper C++ `static inline` del encabezado;
15.7 agregado constante `extern const` (forma de DEFINE_GUID).
Limitaciones documentadas: 15.3 (uniones C++ con miembros no triviales), 15.4 (std::variant), 15.5 (punteros a
vectores fijos de DirectXMath), 15.7 para agregados `static const` internos del encabezado.
"""
from comun import Caso, positivo, fin

H = r'''
struct V2 { float x, y; };
struct Owner { int n; V2 pos; int arr[3]; Owner(); ~Owner(); };
Owner* get_owner();
struct TB { int a, b; };
struct Blend { TB targets[4]; int count; };
Blend* get_blend();
static inline int helper_add(int a, int b) { return a + b; }
struct GUIDx { unsigned long d1; unsigned short d2, d3; unsigned char d4[8]; };
extern const GUIDx IID_Thing;
'''
S = r'''
Owner::Owner() : n(0) {}
Owner::~Owner() {}
static char storage[sizeof(Owner)];
Owner* get_owner() { Owner* o = (Owner*)storage; o->n = 40; o->pos.x = 1; o->pos.y = 1; o->arr[2] = 0; return o; }
static Blend bl = {{{1, 2}, {3, 4}, {5, 6}, {7, 35}}, 4};
Blend* get_blend() { return &bl; }
const GUIDx IID_Thing = {42, 0, 0, {0}};
'''
BODY = r'''
    Owner* o = get_owner();
    if (o->n + (int)(o->pos.x + o->pos.y) + o->arr[2] != 42) return 1;
    o->arr[1] = 5;
    if (o->arr[1] != 5) return 2;
    Blend* b = get_blend();
    if (b->targets[3].b + b->targets[3].a != 42) return 3;
    if (helper_add(40, 2) != 42) return 4;
    if ((int)IID_Thing.d1 != 42) return 5;
    return 42;
'''
c = Caso('t15-cpp', H, S, language='C++')
positivo(c, BODY, modes=['directo', 'dll'])
# EN: A record with its own constructor named by a precompiled generic body is item 13.11. / ES: Un record con
#     constructor propio nombrado por una plantilla precompilada es el item 13.11.
lines = BODY.splitlines()
BODY_LIB = '\n'.join(line for line in lines if 'o->' not in line and 'get_owner' not in line and 'helper_add' not in line)
positivo(c, BODY_LIB, modes=['lib'])
fin()
