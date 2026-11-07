# T08-004: validación de punteros atómicos C

El control C y el consumidor NativeSharp actualizaron y leyeron punteros
atómicos internos a un struct C y a _Bool, el segundo con almacenamiento TLS.
Ambas variantes de referencia, estática y DLL, pasaron O0–O3. Los IR de C,
NativeSharp y NativeSharp con referencia DLL comprobaron cargas y escrituras
secuencialmente consistentes y alineación 8. Evidencia:
`A:\nsc-interop-build\tanda08-static-contratos-1325-1505`.

La construcción incremental terminó con dos advertencias previas de
inicializadores en CppSemanticInventory. Pasaron las regresiones de globales
internas TLS, punteros y agregados, y las de punteros atómicos de anchos
adicionales globales y TLS y apuntados calificados.

El cambio admite también otros apuntados proyectables, pero este control no
certifica enum, void, calificadores ni operaciones compuestas de los tipos
nuevos. La tanda y su validación integral siguen abiertas.
