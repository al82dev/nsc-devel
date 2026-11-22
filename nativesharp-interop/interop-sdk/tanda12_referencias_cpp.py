"""Tanda 12: referencias, receptores y procedencia (items 12.1-12.8).

12.1 metodos con receptor `&` y `const&` (simdjson `parse() &`, `value() &` / `const&`), con la sobrecarga `&&`
eliminada; 12.2 retorno `int&&` leido en un local; 12.6 referencia a arreglo `const int (&)[3]`; 12.7 `T&` en un
metodo static, asignando por la referencia devuelta; 12.8 `const B&` con un derivado.
Limitaciones documentadas: 12.3 (receptor volatile, solo Boost), 12.4/12.5 (procedencia exacta del retorno por
referencia: requiere los contratos de D6).
"""
from comun import Caso, positivo, fin

H = r'''
struct Parser { int n; int parse(int x) & { return x + n; } int parse(int x) && = delete; int peek() const& { return n; } };
struct Opt { int v; int& value() & { return v; } const int& value() const& { return v; } };
struct Holder { int v; int&& take() { return static_cast<int&&>(v); } };
struct Pick { static int& first(int& a, int& b) { return a; } };
int sum_arr(const int (&a)[3]);
struct B { int x; };
struct D : B { int y; };
int read_base(const B& b);
'''
S = r'''
int sum_arr(const int (&a)[3]) { return a[0] + a[1] + a[2]; }
int read_base(const B& b) { return b.x; }
'''
BODY = r'''
    Parser p; p.n = 40;
    if (p.parse(2) != 42 || p.peek() != 40) return 1;
    Opt o; o.v = 41; o.value() = 42;
    if (o.value() != 42) return 2;
    Holder h; h.v = 42; int x = h.take();
    if (x != 42) return 3;
    int a = 40; int b = 2; Pick.first(ref a, ref b) = 42;
    if (a != 42) return 4;
    int[3] arr; arr[0] = 40; arr[1] = 1; arr[2] = 1;
    if (sum_arr(in arr) != 42) return 5;
    D d; d.x = 42; d.y = 0;
    if (read_base(in d) != 42) return 6;
    return 42;
'''
c = Caso('t12-cpp', H, S, language='C++')
positivo(c, BODY, modes=['directo', 'dll'])
# EN: A derived-to-base reference inside a precompiled generic body has no live upcast observation (item 13.11).
# ES: Una referencia de derivado a base dentro de una plantilla precompilada no tiene la observacion en vivo del
#     ajuste a la base (item 13.11).
positivo(c, BODY.replace('    if (read_base(in d) != 42) return 6;', '    if (d.x != 42) return 6;'), modes=['lib'])
fin()
