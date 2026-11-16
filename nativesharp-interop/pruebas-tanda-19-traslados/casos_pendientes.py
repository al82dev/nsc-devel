"""T19-036: casos de traslados que todavia no funcionan; no es un validador de la bateria.

Cada entrada tiene la forma de `CASES` en validar.py y pasa a ese archivo cuando NativeSharp la consume como
Clang (registro t19-traslados-verificacion.md). `anidado-plantilla-var` usa el encabezado y la fuente de
`anidado-plantilla`.
"""
PENDING = {
    'anidado-plantilla': {
        'header': '''namespace T {
template <class V> struct Box {
    struct Item { V v; int tag; };
    static Item make(V v) noexcept { Item r; r.v = v; r.tag = 3; return r; }
};
API int seen(Box<int>::Item item) noexcept;
}
''',
        'source': 'namespace T { int seen(Box<int>::Item item) noexcept { return item.v * 100 + item.tag; } }\n',
        'cpp': 'T::Box<int>::Item it = T::Box<int>::make(4); it.tag = it.tag + 1; return T::seen(it) + it.v * 10;',
        'ns': 'T.Box<int>.Item it = T.Box<int>.make(4); it.tag = it.tag + 1; return T.seen(it) + it.v * 10;',
        'crt': False,
    },
    'anidado-plantilla-var': {
        'header': None,
        'source': None,
        'cpp': 'auto it = T::Box<int>::make(4); it.tag = it.tag + 1; return T::seen(it) + it.v * 10;',
        'ns': 'var it = T.Box<int>.make(4); it.tag = it.tag + 1; return T.seen(it) + it.v * 10;',
        'crt': False,
    },
}
