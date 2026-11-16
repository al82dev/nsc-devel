CASES['intlong'] = ('C++', 'c++20', 'namespace A { int g(int x); int g(long x); }\n',
                    'int Main() { return A.g(5); }\n', 'namespace A { int g(int x) { return 1; } int g(long x) { return 2; } }\n')
CASES['shortlong'] = ('C++', 'c++20', 'namespace A { int g(int x); int g(long x); }\n',
                      'int Main() { short s = 5; return A.g(s); }\n', 'namespace A { int g(int x) { return 1; } int g(long x) { return 2; } }\n')
CASES['uint'] = ('C++', 'c++20', 'namespace A { int g(unsigned x); int g(unsigned long x); }\n',
                 'int Main() { uint u = 5; return A.g(u); }\n', 'namespace A { int g(unsigned x) { return 1; } int g(unsigned long x) { return 2; } }\n')
CASES['longlong'] = ('C++', 'c++20', 'namespace A { int g(long x); int g(long long x); }\n',
                     'int Main() { long v = 5; return A.g(v); }\n', 'namespace A { int g(long x) { return 1; } int g(long long x) { return 2; } }\n')
CASES['amb-int'] = ('C++', 'c++20', 'namespace A { int g(long x); int g(long long x); }\n',
                    'int Main() { return A.g(5); }\n', 'namespace A { int g(long x) { return 1; } int g(long long x) { return 2; } }\n')
CASES['fnptr-null'] = ('C', 'c17', 'typedef int (*cb)(int);\ntypedef struct S { cb f; int v; } S;\nint call(cb f, int v);\n',
                       'int Main() { S s; s.f = null; return call(null, 3); }\n', 'int call(cb f, int v) { return f ? f(v) : v + 40; }\n')
CASES['alias-tipo'] = ('C++', 'c++20', 'namespace B { struct P { int x; }; inline int get(P p) { return p.x; } }\nnamespace A = B;\n',
                       'int Main() { A.P p; p.x = 7; return A.get(p); }\n', '')
CASES['ref-ret'] = ('C++', 'c++20', 'namespace A { inline const int& pick(const int& a, const int& b) { return a < b ? b : a; } }\n',
                    'int Main() { int a = 3; int b = 9; return A.pick(in a, in b); }\n', '')
CASES['new-cpp'] = ('C++', 'c++20', 'namespace A { struct K { int v; K(int x) : v(x) {} ~K() {} int get() const { return v; } }; }\n',
                    'int Main() { A.K k = new A.K(4); return k.get(); }\n', '')
