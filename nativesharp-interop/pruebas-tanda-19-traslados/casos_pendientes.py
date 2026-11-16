"""T19-036: casos de traslados que todavia no funcionan; no es un validador de la bateria.

Cada entrada tiene la forma de `CASES` en validar.py y pasa a ese archivo cuando NativeSharp la consume como
Clang (registro t19-traslados-verificacion.md):

- `globales-precompilada`: globales C++ de namespace y campos estaticos usados desde una plantilla NativeSharp
  precompilada. El consumidor dice `unknown identifier 'T'`: el catalogo precompilado solo transporta globales C.
"""
PENDING = {
    'globales-precompilada': {
        'header': '''namespace T {
struct P { int x; int y; };
API extern int counter;
API extern P origin;
struct S {
    API static int total;
    API static P corner;
};
API int bump() noexcept;
}
''',
        'source': '''namespace T {
int counter = 3;
P origin = {1, 2};
int S::total = 10;
P S::corner = {5, 6};
int bump() noexcept { counter = counter + 1; S::total = S::total + counter; return S::total; }
}
''',
        'cpp': 'T::counter = 7; T::origin.y = T::origin.y + 1; T::S::total = 20; int b = T::bump(); '
               'return b * 1000 + T::origin.y * 100 + T::S::corner.x * 10 + T::counter;',
        'ns': 'T.counter = 7; T.origin.y = T.origin.y + 1; T.S.total = 20; int b = T.bump(); '
              'return b * 1000 + T.origin.y * 100 + T.S.corner.x * 10 + T.counter;',
        'crt': False,
        'ir': True,
    },
}
