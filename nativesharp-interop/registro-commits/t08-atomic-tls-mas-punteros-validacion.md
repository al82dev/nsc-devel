# T08: validación de punteros atómicos TLS adicionales

El build incremental de `nsc` terminó con código 0 y dos advertencias de
inicializadores de `CppSemanticInventory.cpp`, no investigadas aquí. El control
`validar-punteros-tls.cmd` pasó O0–O3 para punteros TLS atómicos no calificados
a float, long y unsigned long C; el C observó los valores resultantes. Se
verificó el IR de acceso atómico y la declaración TLS del puntero float.

La regresión `pruebas-tanda-08-tls-c/atomic/validar.cmd` terminó con código 0.
El control se añadió al gate agrupado, aún pendiente de repetición. No acredita
aislamiento por hilos ni apuntados calificados de estos tipos. T08-004 y la
tanda 8 siguen abiertos; NSLink y metadata no se modificaron.
