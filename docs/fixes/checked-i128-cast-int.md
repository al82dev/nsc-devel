# Conversión checked de i128 a int

En `54fd1741f`, convertir mediante `checked((int)value)` un `i128` justo
encima o debajo del rango de `int` truncaba y continuaba en O0/O2. El control
representable pasó. Comandos y resultados previos se conservan en
`A:/nsc-fixes-evidencia/i128-checked-cast-before-20261112.log`.

Para esta conversión concreta, `convertScalar` trunca, extiende de nuevo con
signo y compara con el original. Si difieren, emite el trap de overflow
entero existente, código 147. Las conversiones ordinarias y las demás parejas
de tipos conservan su comportamiento. No se modificó el ejemplo original.

El build incremental terminó 0. Compilador aislado SHA-256
`4cf930e9e2de5f4585a6fa752bf44d9d47d372aa6a8c1bd7cf1de964f84f1d3f`.
La regresión en O0/O2 cubre ambos valores fuera de rango, ambos límites,
cero, menos uno y truncamiento ordinario. Las cinco pruebas dirigidas de
aritmética y conversión `checked` i128 pasaron 5/5; evidencia en
`A:/nsc-fixes-evidencia/i128-checked-cast-ctest-20261112.log`. No acredita
otras conversiones, plataformas ni combinaciones. `CMakeLists.txt` y
`OopLlvmFieldsAndConversions.cpp` se comparten con la rama de interoperabilidad
y requieren revisión al integrarse. No se ejecutó batería completa.
