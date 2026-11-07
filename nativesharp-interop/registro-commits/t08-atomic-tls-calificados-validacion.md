# T08: validación TLS de apuntados atómicos calificados adicionales

La construcción incremental de `nsc` pasó con dos advertencias de
inicializadores en `CppSemanticInventory.cpp`, no investigadas aquí. El control
`validar-calificados-tls.cmd` pasó O0–O3 para apuntados const y volatile de
float, long y unsigned long C. Se contrastó el IR volatile de Clang y NSC,
las cargas y escrituras atómicas de los punteros y los rechazos de pérdida de
calificadores en los casos ensayados.

Pasaron las regresiones `validar-punteros-tls.cmd`,
`validar-mas-calificados.cmd` y `pruebas-tanda-08-tls-c/atomic/validar.cmd`.
El control nuevo se agregó al gate agrupado, aún no repetido. No acredita
aislamiento entre hilos para estos seis apuntados, DLL ni todos los escapes.
T08-004 y la tanda 8 siguen abiertos; NSLink y metadata no se modificaron.
