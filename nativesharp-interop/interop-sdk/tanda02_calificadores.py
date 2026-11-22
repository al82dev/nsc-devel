"""Tanda 2: calificadores por nivel de puntero (items 2.1-2.9).

2.1 parametros `const char**`, `const char* const*`, `int *const *`; 2.2 retorno `const char**`;
2.3 campos C `const char**` y `char const* const*`, y el campo C++ `char const* const*` (Slang `searchPaths`);
2.4 callback C con `const unsigned char* const*`; 2.5 `const char*&` en funcion y metodo C++;
2.6 `int *const p` e `int a[const 4]`; 2.8 la identidad sobrevive al productor precompilado (.lib y DLL);
2.9 `T***` y `void**` (ya funcionaba: se conserva la prueba).
Rechazos (O0): escribir a traves de la base const, pasar `sbyte**` a `const char**` (regla T08-D17), y
2.7 `volatile int**`, que NativeSharp no puede expresar (limitacion documentada).
"""
from comun import Caso, positivo, rechazo, fin

H = r'''
typedef struct Ev { const char** mime_types; int n; char const* const* paths; } Ev;
int count_cpp(const char** pp);
int count_cpcp(const char* const* pp, int n);
int read_icp(int *const *pp);
const char** names(void);
int store(const char** out);
typedef int (*cb_t)(const unsigned char* const* rows, int n);
int with_cb(cb_t cb);
Ev* make_ev(void);
int f_top(int *const p);
int g_top(int a[const 4]);
typedef struct D { int v; } D;
int scan(D*** out);
int wait_all(void** items, int n);
int rv(volatile int **pp);
'''
S = r'''
static const char* table[] = {"ab", "cde", 0};
static const char* types[] = {"x/y", 0};
static Ev ev = {types, 1, table};
int count_cpp(const char** pp) { int n = 0; while (pp[n]) n++; return n; }
int count_cpcp(const char* const* pp, int n) { int t = 0; for (int i = 0; i < n; i++) { const char* s = pp[i]; while (*s++) t++; } return t; }
int read_icp(int *const *pp) { return **pp; }
const char** names(void) { return table; }
int store(const char** out) { *out = "hello"; return 5; }
static const unsigned char r0[] = {1, 2}; static const unsigned char r1[] = {3, 4};
int with_cb(cb_t cb) { const unsigned char* rows[] = {r0, r1}; return cb(rows, 2); }
Ev* make_ev(void) { return &ev; }
int f_top(int *const p) { return *p; }
int g_top(int a[const 4]) { return a[3]; }
static D d = {42}; static D* arr[] = {&d}; static D** dp = arr;
int scan(D*** out) { *out = dp; return 1; }
int wait_all(void** items, int n) { return n == 2 && items[0] && !items[1]; }
int rv(volatile int **pp) { return **pp; }
'''
PRELUDIO = 'public static class Cb { public static int Rows(byte const** rows, int n) { return rows[0][1] + rows[1][0]; } }'
BODY = r'''
    sbyte const** t = names();
    if (count_cpp(t) != 2) return 1;
    if (count_cpcp(t, 2) != 5) return 2;
    int x = 7; int* px = &x;
    if (read_icp(&px) != 7) return 3;
    sbyte const* s = null;
    if (store(&s) != 5 || s[0] != 104) return 4;
    if (t[1][2] != 101) return 5;
    Ev* e = make_ev();
    if (e->mime_types[0][1] != 47 || e->paths[1][0] != 99) return 6;
    if (with_cb(Cb.Rows) != 5) return 7;
    int y = 40; int[4] a; a[3] = 2;
    if (f_top(&y) + g_top(&a[0]) != 42) return 8;
    D** l = null; scan(&l);
    int z = 1; void*[2] it; it[0] = (void*)&z; it[1] = null;
    if (l[0]->v != 42 || wait_all(&it[0], 2) != 1) return 9;
    return 42;
'''
c = Caso('t2-c', H, S)
positivo(c, BODY, PRELUDIO)
rechazo(c, 'sbyte const** t = names(); t[0][0] = 1; return 0;', 'not assignable')
rechazo(c, 'sbyte* m = null; sbyte** q = &m; return count_cpp(q);', 'count_cpp')
rechazo(c, 'return rv(null);', "('volatile int **') is not yet mapped")

HPP = r'''
struct SessionDesc { char const* const* searchPaths; long long count; };
void fetch(const char*& out, int& n);
struct Sess { int load(const char*& name); static int first(const SessionDesc& d); };
const char** cpp_names();
'''
SPP = r'''
static const char* k = "abc";
void fetch(const char*& out, int& n) { out = k; n = 3; }
int Sess::load(const char*& name) { name = k; return 1; }
int Sess::first(const SessionDesc& d) { return d.searchPaths[1][0]; }
static const char* list[] = {"zero", "abc", nullptr};
const char** cpp_names() { return list; }
'''
BODY_CPP = r'''
    sbyte const* s = null; int n = 0;
    fetch(ref s, ref n);
    Sess q; sbyte const* t = null;
    if (q.load(ref t) != 1) return 1;
    if (s[1] != 98 || n != 3 || t[2] != 99) return 2;
    SessionDesc d; d.searchPaths = cpp_names(); d.count = 2;
    if (d.searchPaths[0][0] != 122 || Sess.first(in d) != 97) return 3;
    return 42;
'''
cpp = Caso('t2-cpp', HPP, SPP, language='C++')
positivo(cpp, BODY_CPP)
fin()
