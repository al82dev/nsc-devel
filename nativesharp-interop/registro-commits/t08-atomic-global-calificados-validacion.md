# T08: validación de apuntados calificados de globales atómicos C

La construcción incremental de `nsc` terminó con código 0 y dos advertencias
de inicializadores en `CppSemanticInventory.cpp`, no investigadas aquí. El
control `validar-calificados.cmd` generó en una carpeta aislada la variante no
TLS de los fixtures TLS existentes. Los apuntados const y volatile de unsigned
int, short, long long y double pasaron O0–O3. Los verificadores existentes
comprobaron el IR volatile directo e indexado; los casos negativos ensayados
rechazaron escritura const y pérdida de calificadores.

La regresión `pruebas-tanda-08-tls-c/atomic/validar.cmd` terminó con código 0.
Se incorporó el control nuevo al gate agrupado, aún pendiente de repetición.
Esto no acredita todos los tipos ni todos los escapes. T08-004 y la tanda 8
siguen abiertas; no se modificó NSLink.
