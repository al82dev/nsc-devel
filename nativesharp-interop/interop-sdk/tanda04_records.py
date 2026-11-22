"""Tanda 4: arreglos, records, uniones y bit-fields C (items 4.1-4.20).

4.1 campo `void* pad[2]`; 4.2 campo `unsigned short (*image)[4]`; 4.3 flexible `void* caps[]`; 4.5 miembro final
`[0]`; 4.6 cola `[1]` por direccion; 4.7 `int[2][4]` hacia `int (*)[4]` e `int[3]` hacia `const int*`; 4.8 `&a[0][0]`;
4.9 global redeclarada `int tbl[]` / `int tbl[3]`; 4.13 store de bit-field de unidad completa (IR en la prueba
dirigida); 4.14 atomico estrecho con signo (sin diferencia con Clang); 4.17 uniones nombradas y anonimas;
4.18 bit-fields con signo y de DBusError; 4.19 campo empaquetado; 4.20 arreglos de records y de char en campos.
Limitaciones documentadas: 4.11 transparent_union, 4.12 ventana de acceso con receptor volatile, 4.15 campos
`_Atomic`, 4.16 barreras de stdatomic.h.
"""
from comun import Caso, positivo, fin

H = r'''
typedef struct S { int a; void* pad[2]; int b; } S;
typedef struct Img { unsigned short (*image)[4]; int n; } Img;
typedef struct Bos { int n; void* caps[]; } Bos;
typedef struct Z { int n; unsigned char data[0]; } Z;
typedef struct T1 { int n; unsigned char data[1]; } T1;
S* mk_s(void);
Img* mk_img(void);
Bos* mk_bos(void);
Z* mk_z(void);
T1* mk_t1(void);
int sumrows(int (*m)[4], int rows);
int sum3(const int* v, int n);
int first(int* p);
extern int tbl[];
extern int tbl[3];
typedef union U { int i; float f; struct { short a, b; } s; } U;
typedef struct Ev { int type; union { int key; double pos; } u; } Ev;
U mku(int v);
int ev(Ev* e);
typedef struct DBusError { const char* name; const char* message; unsigned int d1 : 1; unsigned int d2 : 1;
                           unsigned int d3 : 1; unsigned int d4 : 1; unsigned int d5 : 1; void* padding1; } DBusError;
typedef struct H { int s : 3; unsigned u : 5; } H;
int chk(DBusError* e, H* h);
typedef struct P { int x, y; } P;
typedef struct Pal { P entries[4]; char expire[18]; int n; } Pal;
int use_pal(Pal* p);
#pragma pack(push, 1)
typedef struct Pk { char c; int v; } Pk;
#pragma pack(pop)
int pk_v(Pk* k);
extern _Atomic signed char sc;
int rd_sc(void);
'''
S = r'''
static int x = 1;
static S s = {40, {&x, 0}, 2};
S* mk_s(void) { return &s; }
static unsigned short px[2][4] = {{1, 2, 3, 4}, {5, 6, 7, 40}};
static Img im = {px, 2};
Img* mk_img(void) { return &im; }
static struct { int n; void* caps[2]; } bos = {42, {0, 0}};
Bos* mk_bos(void) { return (Bos*)&bos; }
static struct { int n; unsigned char d[4]; } zz = {40, {0, 2, 0, 0}};
Z* mk_z(void) { return (Z*)&zz; }
static struct { int n; unsigned char d[4]; } t1 = {40, {0, 0, 2, 0}};
T1* mk_t1(void) { return (T1*)&t1; }
int sumrows(int (*m)[4], int rows) { int t = 0; for (int i = 0; i < rows; i++) t += m[i][3]; return t; }
int sum3(const int* v, int n) { int t = 0; for (int i = 0; i < n; i++) t += v[i]; return t; }
int first(int* p) { *p = 42; return 0; }
int tbl[3] = {40, 1, 1};
U mku(int v) { U u; u.i = v; return u; }
int ev(Ev* e) { return e->type + e->u.key; }
int chk(DBusError* e, H* h) { return e->d1 + e->d3 * 2 + (h->s == -2 ? 10 : 0) + h->u; }
int use_pal(Pal* p) { return p->entries[3].y + p->expire[17] + p->n; }
int pk_v(Pk* k) { return k->v; }
_Atomic signed char sc = -3;
int rd_sc(void) { return sc; }
'''
BODY = r'''
    S* s = mk_s();
    if (s->pad[0] == null || s->pad[1] != null || s->a + s->b != 42) return 1;
    Img* m = mk_img();
    if (m->image[1][3] + m->image[0][1] != 42) return 2;
    Bos* b = mk_bos();
    if (b->caps[1] != null || b->n != 42) return 3;
    Z* z = mk_z();
    if (z->n + z->data[1] != 42) return 4;
    T1* t = mk_t1(); byte* d = &t->data[0];
    if (t->n + d[2] != 42) return 5;
    int[2][4] a; a[0][3] = 30; a[1][3] = 2; int[3] v; v[0] = 4; v[1] = 3; v[2] = 3;
    if (sumrows(a, 2) + sum3(v, 3) != 42) return 6;
    int[3] w; first(&w[0]); first(&a[0][0]);
    if (w[0] != 42 || a[0][0] != 42) return 7;
    if (tbl[0] + tbl[1] + tbl[2] != 42) return 8;
    U u = mku(40); Ev e; e.type = 2; e.u.key = u.i;
    if (ev(&e) != 42) return 9;
    DBusError de; de.d1 = 1u; de.d2 = 0u; de.d3 = 1u; de.d4 = 0u; de.d5 = 0u; H h; h.s = -2; h.u = 27u;
    if (chk(&de, &h) != 40) return 10;
    Pal p; p.entries[3].y = 30; p.expire[17] = 10; p.n = 2;
    if (use_pal(&p) != 42) return 11;
    Pk k; k.c = 1; k.v = 42;
    if (k.v != 42 || pk_v(&k) != 42) return 12;
    sc += 1;
    if (rd_sc() != -2) return 13;
    return 42;
'''
positivo(Caso('t4-c', H, S), BODY)
fin()
