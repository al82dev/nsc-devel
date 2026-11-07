# T08: validación de globales C atómicos adicionales

El build incremental de `nsc` pasó con dos advertencias de inicializadores en
`CppSemanticInventory.cpp`, no investigadas en esta validación. El control
`pruebas-tanda-08-atomic-global-matrix/validar-extra.cmd` pasó O0–O3 para dos
enteros estrechos y seis punteros, con lecturas y escrituras cruzadas con C.
Se comprobaron cargas y escrituras `seq_cst` en el IR de Clang y NSC para los
ocho globales. Se rechazaron la escritura al apuntado const y la pérdida de
calificadores const y volatile en los casos ensayados.

La regresión `pruebas-tanda-08-tls-c/atomic/validar.cmd` pasó. El control se
incorporó al gate agrupado, aún no repetido. No se acreditan las variantes de
otros apuntados calificados, operaciones compuestas o bibliotecas precompiladas
y DLL. T08-004 y la tanda siguen abiertas; NSLink no se modificó.
