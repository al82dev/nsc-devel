# T08-010: calificadores en miembros flexibles C

Los miembros flexibles C `const int[]` y `volatile int[]` conservan sus
calificadores al importarse. El acceso indexado respeta la prohibición de
escritura y de obtener un puntero mutable a un elemento const. Los accesos
volatile conservan la semántica de orden de Microsoft cuando corresponde.

Se construyó NSC incrementalmente y se ejecutó el fixture de arreglos flexibles
en O0–O3, con inclusión directa y consumidores precompilados con encabezado y
solo metadata. Se contrastó el IR de lectura y escritura volatile con Clang
en las tres rutas; se comprobaron los diagnósticos de escritura y escape de
puntero const. Pasaron también las regresiones dirigidas de `_Bool`, layout,
records C y campos de bits volatile, además de `nsc_cpp_session_tests`.

La evidencia de esta ronda está en
`A:/nsc-interop-build/tanda08-flexible-calificados-20261107c`. La comparación
de IR se realizó con el control de Clang en O1; la ejecución se comprobó en
O0–O3. Esto no acredita todas las combinaciones de tipos o calificadores. La
conversión implícita del arreglo flexible a puntero permanece pendiente. La
tanda 8 continúa abierta y todavía requiere su validación completa.
