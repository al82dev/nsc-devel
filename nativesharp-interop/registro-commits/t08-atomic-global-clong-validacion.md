# T08: validación de long C atómico global en Windows x64

La construcción incremental de `nsc` terminó con código 0 y dos advertencias
de inicializadores en `CppSemanticInventory.cpp`, no investigadas aquí. La
matriz `validar.cmd` pasó O0–O3 para `long` y `unsigned long` C no TLS de 32
bits, incluyendo lecturas, escrituras, operaciones compuestas y observación C.
El IR de Clang y NSC pasó los verificadores de accesos y actualizaciones
atómicas `seq_cst` de la matriz.

No se repitió la regresión TLS en esta ejecución; el cambio mantiene la
condición anterior de la ruta TLS. El gate agrupado sigue pendiente y T08-004
no se cierra. NSLink no se modificó.
