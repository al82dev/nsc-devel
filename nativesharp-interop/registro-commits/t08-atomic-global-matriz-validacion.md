# T08: validación agrupada de globales C atómicos no TLS

La construcción incremental de `nsc` terminó con código 0 y dos advertencias
de inicializadores ausentes en `CppSemanticInventory.cpp`, cuyo origen no se
evaluó en esta validación. `pruebas-tanda-08-atomic-global-matrix/validar.cmd`
pasó O0–O3 con lecturas y escrituras observadas desde C y NativeSharp. Se
comprobaron cargas y escrituras `seq_cst` en el IR de Clang y NSC para los
nueve globales de la matriz.

La regresión `pruebas-tanda-08-tls-c/atomic/validar.cmd` terminó con código 0.
El nuevo control se agregó al gate de la tanda, que aún no se ha repetido.
Esto no acredita otros tipos, operaciones compuestas, concurrencia entre hilos,
publicación precompilada ni DLL para esta matriz. T08-004 y la tanda siguen
abiertas. NSLink no se modificó.
