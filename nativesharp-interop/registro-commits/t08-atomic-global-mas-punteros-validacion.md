# T08: validación de punteros atómicos globales adicionales

El build incremental de `nsc` terminó con código 0; se observaron dos
advertencias de inicializadores en `CppSemanticInventory.cpp`, no investigadas
aquí. `validar-mas-punteros.cmd` pasó O0–O3 para `_Atomic(float*)`,
`_Atomic(long*)` y `_Atomic(unsigned long*)` no TLS y sin calificar, con
lectura, asignación, avance y observación desde C. El IR de NSC pasó el control
de carga, escritura y actualización atómicas `seq_cst` por global.

La regresión `pruebas-tanda-08-tls-c/atomic/validar.cmd` terminó con código 0.
El control se añadió al gate de la tanda, que todavía no se ha repetido. No se
acreditan apuntados calificados ni TLS para estos tres tipos. T08-004 y la tanda
siguen abiertas; NSLink no se modificó.
