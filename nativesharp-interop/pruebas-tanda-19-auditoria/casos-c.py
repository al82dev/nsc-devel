CASES['complex'] = ('C', 'c17', 'double cre(double _Complex z);\n', 'int Main() { return 0; }\n', 'double cre(double _Complex z) { return __real__ z; }\n')
CASES['vla'] = ('C', 'c17', 'int suma(int n, int a[n]);\n', 'int Main() { int* a = stackalloc int[3]; a[0] = 1; a[1] = 2; a[2] = 3; return suma(3, a); }\n',
                'int suma(int n, int a[n]) { int s = 0; for (int i = 0; i < n; ++i) s += a[i]; return s; }\n')
CASES['macro-fn'] = ('C', 'c17', '#define DOBLE(x) ((x) * 2)\n', 'int Main() { return DOBLE(21); }\n', '')
CASES['bitfield-enum'] = ('C++', 'c++20', 'enum class E : unsigned { a, b, c };\nstruct S { E e : 2; unsigned v : 6; };\n',
                          'int Main() { S s; s.v = 5; return (int)s.v; }\n', '')
CASES['union-cpp'] = ('C++', 'c++20', 'union U { int i; float f; };\n', 'int Main() { U u; u.i = 9; return u.i; }\n', '')
CASES['dtor-throw'] = ('C++', 'c++20', 'namespace A { struct K { int v = 3; ~K() noexcept(false); }; }\n',
                       'int Main() { A.K k = new A.K(); return k.v; }\n', 'namespace A { K::~K() noexcept(false) {} }\n')
CASES['privado'] = ('C++', 'c++20', 'namespace A { class K { int secret = 1; public: int open = 2; }; }\n',
                    'int Main() { A.K k = new A.K(); return k.secret; }\n', '')
CASES['op-miembro'] = ('C++', 'c++20', 'namespace A { struct V { int x; V& operator+=(int d) { x += d; return *this; } int operator[](int i) const { return x + i; } }; }\n',
                       'int Main() { A.V v; v.x = 1; return v[2]; }\n', '')
CASES['base-virtual'] = ('C++', 'c++20', 'namespace A { struct B { int b = 4; }; struct D : virtual B { int d = 1; }; }\n',
                         'int Main() { A.D d = new A.D(); return d.b; }\n', '')
CASES['var-template'] = ('C++', 'c++20', 'namespace A { template <class T> constexpr T cero = T(7); }\n',
                         'int Main() { return A.cero<int>; }\n', '')
CASES['std-pair'] = ('C++', 'c++20', '#include <utility>\n', 'int Main() { std.pair<int, int> p = new std.pair<int, int>(3, 4); return p.first + p.second; }\n', '')
CASES['ref-ret-mut'] = ('C++', 'c++20', 'namespace A { struct V { int x; int& at() { return x; } }; }\n',
                        'int Main() { A.V v; v.x = 6; return v.at(); }\n', '')
