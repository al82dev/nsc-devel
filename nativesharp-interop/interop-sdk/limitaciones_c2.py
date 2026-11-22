"""Limitaciones C2: macros C y contextos sintacticos (plan de limitaciones, tareas C2-01..C2-09).

C2-01/C2-02: una macro de objeto que se expande a una lista de argumentos (`AI_MATKEY_COLOR`, las macros URL de
opusfile) equivale, como argumento de una llamada, a todos sus elementos en orden, como la expande el preprocesador C;
fuera de una llamada da un diagnostico propio y nunca se toma solo el ultimo valor.
"""
from comun import Caso, positivo, rechazo, diagnostico, fin

H = r'''
#define MATKEY_COLOR "$clr.diffuse", 0, 0
#define PAIR 20, 22
#define NESTED (1, 2), 40
int key_sum(const char* key, unsigned type, unsigned index);
int add(int a, int b);
int sum3(int a, int b, int c);
int vsum(int n, ...);
'''
S = r'''
#include <stdarg.h>
int key_sum(const char* key, unsigned type, unsigned index) { int n = 0; while (key[n]) n++; return n + (int)type + (int)index; }
int add(int a, int b) { return a + b; }
int sum3(int a, int b, int c) { return a + b + c; }
int vsum(int n, ...) { va_list list; va_start(list, n); int total = 0; for (int i = 0; i < n; i++) total += va_arg(list, int); va_end(list); return total; }
'''
BODY = r'''
    if (add(PAIR) != 42) return 1;
    if (sum3(0, PAIR) != 42) return 2;
    if (key_sum(MATKEY_COLOR) != 12) return 3;
    if (vsum(2, PAIR) != 42) return 4;
    if (add(NESTED) != 42) return 5;
    return 42;
'''
c = Caso('c2-lista', H, S)
positivo(c, BODY)
rechazo(c, 'int x = PAIR; return x;', 'expands to a list of 2 arguments')
# NativeSharp lookup wins inside a unit that declares the name (no silent C preprocessing of NativeSharp code).
rechazo(c, 'int PAIR = 1; return add(PAIR);', 'add')

# C2-08: constant macros inside a precompiled generic body reach the consumer (the .lib mode instantiates the body in
# a separate project without the header): the consumer evaluates them over the producer's macro source.
H8 = r'''
#define ANSWER 40
#define HALF 2.5
#define LETTER 'A'
#define NAME "ab"
#define BIG 0xFFFFFFFFu
int twice(int v);
'''
S8 = 'int twice(int v) { return 2 * v; }\n'
BODY8 = r'''
    double h = HALF;
    sbyte const* name = NAME;
    uint big = BIG;
    if ((int)(h * 2.0) != 5 || LETTER != 65 || name[1] != 98 || big != 4294967295u) return 1;
    return twice(ANSWER / 2) + 2;
'''
positivo(Caso('c2-constantes', H8, S8), BODY8)

# C2-03: brace-initializer macros (LZ4F_INIT_PREFERENCES, DBUS_ERROR_INIT) initialize a declaration of a record type
# Clang accepts them for, with nested fields; any other use, or a type Clang rejects, is reported.
H3 = r'''
typedef struct Prefs { int level; int flags; struct { int a; int b; } inner; } Prefs;
typedef struct Err { const char* name; const char* message; unsigned dummy1; } Err;
#define PREFS_INIT { 3, 0, { 7, 9 } }
#define ERR_INIT { 0, 0, 1u }
int prefs_sum(const Prefs* p);
'''
S3 = 'int prefs_sum(const Prefs* p) { return p->level + p->flags + p->inner.a + p->inner.b; }\n'
BODY3 = r'''
    Prefs p = PREFS_INIT;
    if (prefs_sum(&p) != 19) return 1;
    Err e = ERR_INIT;
    if (e.name != null || e.message != null || e.dummy1 != 1u) return 2;
    p.level = 26;
    return prefs_sum(&p) - p.level + 26 + 0 * (int)e.dummy1 + 0;
'''
c3 = Caso('c2-llaves', H3, S3)
positivo(c3, BODY3.replace('return prefs_sum(&p) - p.level + 26 + 0 * (int)e.dummy1 + 0;', 'return prefs_sum(&p) - 0;')
         .replace('if (prefs_sum(&p) != 19) return 1;', 'if (prefs_sum(&p) != 19) return 1;'), )
rechazo(c3, 'int x = PREFS_INIT; return x;', "brace initializer list")
rechazo(c3, 'Prefs p = PREFS_INIT; p = PREFS_INIT; return 0;', "brace initializer list")
rechazo(c3, 'Err e = PREFS_INIT; return 0;', "'Prefs'")

# C2-04: a function-like macro in an array dimension (LZ4_COMPRESSBOUND) is evaluated by Clang as an integer constant
# expression; nested macro calls too. A macro whose expansion is not a constant keeps the array invalid.
H4 = r'''
#define BOUND(n) ((n) + ((n) / 255) + 16)
#define TWICE(x) (2 * (x))
#define BASE 10
int runtime_value(void);
#define RUNTIME(n) ((n) + runtime_value())
int fill(int* p, int n);
'''
S4 = 'int runtime_value(void) { return 1; }\nint fill(int* p, int n) { p[0] = 1; p[n - 1] = 2; return n; }\n'
BODY4 = r'''
    int[BOUND(64)] a;
    int[TWICE(BOUND(BASE))] b;
    if (fill(&a[0], 80) != 80 || a[79] != 2) return 1;
    if (fill(&b[0], 52) != 52 || b[51] != 2) return 2;
    return 42;
'''
c4 = Caso('c2-dimension', H4, S4)
positivo(c4, BODY4)
rechazo(c4, 'int[RUNTIME(2)] c; return 0;', 'RUNTIME')

# C2-05: a macro that evaluates an argument several times or conditionally accepts an argument without side effects
# (C gives the same result when it is evaluated once); calls and volatile reads keep the rejection.
H5 = r'''
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define SQUARE(x) ((x) * (x))
typedef struct Pt { int x; int y; } Pt;
extern volatile int shared;
int next(void);
'''
S5 = 'volatile int shared = 3;\nstatic int counter = 0;\nint next(void) { return ++counter; }\n'
BODY5 = r'''
    int a = 3; int b = 5;
    Pt p; p.x = 2; p.y = 7;
    if (MAX(a + 1, b * 2) != 10) return 1;
    if (SQUARE((a + b)) != 64) return 2;
    if (MAX(p.x, p.y) != 7 || SQUARE((long)a) != 9) return 3;
    if (MAX(a > b ? a : b, 4) != 5) return 4;
    return 42;
'''
c5 = Caso('c2-efectos', H5, S5)
positivo(c5, BODY5)
rechazo(c5, 'return SQUARE(next());', 'may have side effects')
rechazo(c5, 'int a = 1; return MAX(a, next());', 'may have side effects')
rechazo(c5, 'return SQUARE(shared);', 'may have side effects')
rechazo(c5, 'int a = 1; return SQUARE(a++);', 'may have side effects')

# C2-06: `#__VA_ARGS__` and `a##__VA_ARGS__` take the written tokens; type arguments, statement macros that modify
# lvalues and macros that expand __FILE__/__LINE__ describe the NativeSharp call site, as C describes the invocation.
H6 = r'''
#define STR(...) #__VA_ARGS__
#define CAT(a, ...) a##__VA_ARGS__
#define FIRST(a, ...) (a)
#define COUNT(...) (sizeof((int[]){__VA_ARGS__}) / sizeof(int))
#define SIZE(T) ((int)sizeof(T))
#define SWAP(a, b) do { int t_ = (a); (a) = (b); (b) = t_; } while (0)
#define WHERE() __LINE__
#define FILE_OF() __FILE__
#define NAME_OF() __func__
int value12;
int str_len(const char* s);
int ends_main(const char* s);
'''
S6 = r'''
int value12 = 12;
int str_len(const char* s) { int n = 0; while (s[n]) n++; return n; }
int ends_main(const char* s) { int n = str_len(s); return n >= 7 && s[n - 7] == 'M' && s[n - 1] == 's'; }
'''
# The body starts at line 4 of Main.ns in direct mode (using, preludio, `int Main()`, `{`), so WHERE() on the first
# body line is 5.
BODY6 = r'''    int line = WHERE();
    if (str_len(STR(a, b)) != 4 || CAT(value, 12) != 12 || FIRST(40, 1, 2) != 40) return 1;
    if ((int)COUNT(1, 2, 3) != 3 || SIZE(long) != 8) return 2;
    int a = 1; int b = 2; SWAP(ref a, ref b);
    if (a != 2 || b != 1) return 3;
    if (line != 5 || ends_main(FILE_OF()) != 1) return 4;
    return 42;
'''
c6 = Caso('c2-preprocesador', H6, S6)
positivo(c6, BODY6, modes=['directo'])
positivo(Caso('c2-preprocesador-bib', H6, S6), BODY6.replace('if (line != 5 || ends_main(FILE_OF()) != 1) return 4;',
                                                           'if (line <= 0 || str_len(FILE_OF()) == 0) return 4;'),
         modes=['lib', 'dll'])
rechazo(c6, 'return str_len(NAME_OF());', 'names the enclosing C function')
rechazo(c6, 'int a = 1; int b = 2; return (int)SWAP(ref a, ref b);', 'it has no value')

# C2-07: a value macro that is not a constant is evaluated at each read, into the reader's own storage (no static
# shared between threads); an lvalue macro keeps its address accessor.
H7 = r'''
typedef struct Pair { int a; int b; } Pair;
int next_id(void);
Pair make_pair(int a);
extern int store;
#define NEXT_ID (next_id())
#define PAIR_OF(x) make_pair(x)
#define CURRENT (make_pair(next_id()))
#define STORE store
'''
S7 = r'''
static int counter = 0;
int store = 5;
int next_id(void) { return ++counter; }
Pair make_pair(int a) { Pair p = {a, a * 2}; return p; }
'''
BODY7 = r'''
    int a = NEXT_ID;
    int b = NEXT_ID;
    if (b != a + 1) return 1;
    Pair p = CURRENT;
    Pair q = CURRENT;
    if (q.a != p.a + 1 || q.b != q.a * 2) return 2;
    STORE = 40;
    if (store != 40) return 3;
    return 42;
'''
positivo(Caso('c2-accesor', H7, S7), BODY7)

# C2-09: macros of headers reached through IncludeDirectory Kind="User" are imported (constants, function-like
# macros, type names, aliases of functions); those of Kind="System" headers follow the system-header policy and stay
# out, without changing it implicitly.
class CasoIncludes(Caso):
    """A case whose header includes `user/u.h` (Kind=User) and `sys/s.h` (Kind=System)."""

    def native(self, folder):
        (folder / 'user').mkdir(parents=True, exist_ok=True)
        (folder / 'sys').mkdir(parents=True, exist_ok=True)
        (folder / 'user' / 'u.h').write_text(r"""
#define U_ANSWER 40
#define U_TWICE(x) ((x) * 2)
#define U_REAL double
int add_impl(int a, int b);
#define U_ADD add_impl
""", encoding='utf-8')
        (folder / 'sys' / 's.h').write_text('#define S_HIDDEN 7\n', encoding='utf-8')
        return super().native(folder)

    def reference(self, folder):
        library = self.native(folder)
        return (f'<CppLibraryReference Header="api.h" Language="{self.language}" Standard="{self.standard}"{library}>'
                '<IncludeDirectory Path="user" Kind="User" /><IncludeDirectory Path="sys" Kind="System" />'
                '</CppLibraryReference>')


H9 = '#include <u.h>\n#include <s.h>\n'
S9 = 'int add_impl(int a, int b) { return a + b; }\n'
BODY9 = r"""
    U_REAL half = 0.5;
    if ((int)(half * 4.0) != 2) return 1;
    return U_ADD(U_TWICE(U_ANSWER) - 40, 2);
"""
c9 = CasoIncludes('c2-includes', H9, S9, clang_args=('-Iuser', '-Isys'))
positivo(c9, BODY9)
rechazo(c9, 'return S_HIDDEN;', "unknown identifier 'S_HIDDEN'")
fin()
