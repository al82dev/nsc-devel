H = 'namespace B { struct P { int x; }; inline int get(P p) { return p.x; } }\nnamespace A = B;\n'
CASES['tipo-directo'] = ('C++', 'c++20', H, 'int Main() { B.P p; p.x = 7; return B.get(p); }\n', '')
CASES['alias-funcion'] = ('C++', 'c++20', H, 'int Main() { B.P p; p.x = 7; return A.get(p); }\n', '')
CASES['alias-new'] = ('C++', 'c++20', H, 'int Main() { A.P p = new A.P(); p.x = 7; return B.get(p); }\n', '')
CASES['alias-var'] = ('C++', 'c++20', H, 'int Main() { var p = new A.P(); p.x = 7; return B.get(p); }\n', '')
CASES['alias-param'] = ('C++', 'c++20', H, 'int F(A.P p) { return p.x; }\nint Main() { B.P p; p.x = 7; return F(p); }\n', '')
CASES['fnptr-cmp'] = ('C', 'c17', 'typedef int (*cb)(int);\ntypedef struct S { cb f; int v; } S;\nint call(cb f, int v);\n',
                      'int Main() { S s; if (s.f == null) return 5; return 1; }\n', 'int call(cb f, int v) { return f ? f(v) : v + 40; }\n')
