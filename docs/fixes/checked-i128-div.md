# División checked de i128

En la revisión `403260d88`, `checked(minimum / -1)` continuaba con wrapping
en O0 y O2; el cociente representable `minimum / 2` pasó. Comandos y
resultados previos: `A:/nsc-fixes-evidencia/i128-checked-div-before-20261112.log`.

`applyIntegerDivision` comprueba ese único par de operandos para división
firmada i128 checked y emite el trap de overflow entero existente, código
147. La guarda de divisor cero permanece anterior; la división ordinaria,
el resto y los demás tipos conservan sus rutas. No se cambió el ejemplo
original del reporte.

El build incremental terminó 0; el compilador aislado tuvo SHA-256
`2c5cde888f968da9b9e49439f67e1703adbce20c9971ad7231344413d41af2b5`.
Las pruebas registradas de suma, resta, multiplicación y división checked
pasaron 4/4 en `A:/nsc-fixes-evidencia/i128-checked-div-ctest-20261112.log`.
La prueba de división ejecuta en O0/O2 el overflow, divisor cero (129),
valores representables, división ordinaria y resto checked con mínimo y -1.
No acredita otras plataformas o tipos. `CMakeLists.txt` se comparte con la
rama de interoperabilidad y requiere revisión al integrarse. No se ejecutó
batería completa.
