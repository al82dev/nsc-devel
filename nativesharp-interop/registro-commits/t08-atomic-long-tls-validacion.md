# T08: validación de long C atómico TLS

El build incremental de `nsc` terminó con código 0 y dos advertencias de
inicializadores en `CppSemanticInventory.cpp`, no investigadas aquí. El control
`validar-long-tls.cmd` pasó O0–O3 para long y unsigned long C de 32 bits en TLS,
con lecturas, operaciones compuestas y observación desde C. Se comprobó IR TLS
de 32 bits con carga y actualización `seq_cst` para ambos globales.

Pasaron las regresiones de la matriz de globales no TLS y del fixture atómico
TLS existente. El control se incorporó al gate agrupado, pendiente de repetir.
No acredita aislamiento por hilo, DLL o todos los operadores. T08-004 y la
tanda siguen abiertos; NSLink y metadata no se modificaron.
