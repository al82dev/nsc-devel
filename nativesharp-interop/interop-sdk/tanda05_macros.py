"""Tanda 5: macros C (items 5.1-5.11).

5.1 una macro objeto que se expande a una lista de argumentos ya no se importa como el ultimo valor de una expresion
con comas (rechazo); 5.3 argumento con efectos evaluado varias veces o condicionalmente: rechazo con nota, y un local
funciona; 5.5 `_Generic` con `sbyte` elige la rama `char`; 5.7 macro que produce un arreglo (`png_jmpbuf`);
5.8 macro alias de un global puntero a funcion (GLEW); 5.9 nombre de tipo importado como argumento (`SIZE(Pt)`);
5.10 macro que escribe su argumento: rechazo sin `ref`, correcto con `ref`; 5.11 literales `L`, `u8`, `long double`,
caracter y macro vacia; 5.4 macro objeto valor (accesor).
Limitaciones documentadas: 5.1 repartir la lista en argumentos, 5.2 macros de llaves, 5.4 carrera del accesor
estatico, 5.6 politica de macros de sistema, 4.10 macros en la longitud de un arreglo.
"""
from comun import Caso, positivo, rechazo, fin

H = r'''
#define AI_MATKEY_COLOR "$clr.diffuse", 0, 0
int get_prop(const char* key, int type, int index);
int next(void);
#define TWICE(x) ((x) + (x))
int ci(int x);
int cc(char x);
#define KIND(x) _Generic((x), char: cc, default: ci)(x)
typedef struct png { int jb[4]; } png;
#define png_jmpbuf(p) ((p)->jb)
int first(int* b);
typedef int (*PFNX)(int);
extern PFNX __glewX;
#define glX __glewX
typedef struct Pt { int x, y; } Pt;
#define SIZE(T) ((int)sizeof(T))
#define RESET(x) do { (x) = 0; } while (0)
#define U8S u8"ok"
#define WS L"w"
#define LD 2.5L
#define EMPTY
#define CH 'A'
#define ANSWER 42
#define NAME "abc"
#define FLAG (1u << 3)
#define MK(n) val_##n
int val_a(void);
'''
S = r'''
int get_prop(const char* key, int type, int index) { return key[1] == 99 ? 42 : 0; }
static int n = 0;
int next(void) { return ++n; }
int ci(int x) { return 1; }
int cc(char x) { return 42; }
int first(int* b) { return b[3]; }
static int imp(int v) { return v + 1; }
PFNX __glewX = imp;
int val_a(void) { return 42; }
'''
BODY = r'''
    if (get_prop("$clr.diffuse", 0, 0) != 42) return 1;
    int v = next() + 20;
    if (TWICE(v) != 42) return 2;
    sbyte c = 5;
    if (KIND(c) != 42) return 3;
    png p; p.jb[3] = 42;
    if (first(png_jmpbuf(&p)) != 42) return 4;
    if (glX(41) != 42) return 5;
    if (SIZE(Pt) != 8) return 6;
    int w = 5; RESET(ref w);
    if (w != 0) return 7;
    double d = LD; int ch = CH;
    if ((int)(d * 4.0) != 10 || ch != 65) return 8;
    sbyte const* name = NAME;
    if (ANSWER + (int)FLAG - 8 + name[0] - 97 != 42) return 9;
    if (MK(a)() != 42) return 10;
    return 42;
'''
c = Caso('t5-c', H, S)
positivo(c, BODY, modes=['directo', 'dll'])
# EN: In a precompiled generic body, constant macros and type-name macro arguments do not reach the consumer: the C
#     catalogue does not keep macro constants (blocked with the NSMeta item of the plan). The rest runs there too.
# ES: En el cuerpo de una plantilla precompilada las macros constantes y los argumentos de tipo no llegan al
#     consumidor: el catalogo C no guarda constantes de macro (bloqueado con el item NSMeta del plan).
BODY_LIB = '\n'.join(line for line in BODY.splitlines()
                     if not any(token in line for token in ('SIZE(', 'LD;', 'ANSWER', 'return 6;', 'return 8;', 'return 9;')))
# NSMeta X4-06: macro constants travel with the producer's macro source (C2-02/C2-08) and a type-name argument in a
#     published template is passed as tokens (the producer checked it), so the whole body runs in lib. / Las
#     constantes de macro viajan con la fuente de macros y un nombre de tipo como argumento se pasa como tokens.
positivo(c, BODY, modes=['lib'])
rechazo(c, 'int v = AI_MATKEY_COLOR; return v;', "unknown identifier 'AI_MATKEY_COLOR'")
rechazo(c, 'return TWICE(next());', 'may have side effects that NativeSharp would evaluate only once')
rechazo(c, 'int w = 5; RESET(w); return w;', 'pass it with ref')
fin()
