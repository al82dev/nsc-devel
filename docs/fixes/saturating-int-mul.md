# Multiplicación saturada de int

En `e6b1e5c57`, `saturating(maximum * 2)` producía wrapping en O0/O2;
reproducción y comandos en
`A:/nsc-fixes-evidencia/int-saturating-mul-before-20261112.log`.

La multiplicación int bajo saturating extiende ambos operandos con signo a
64 bits, calcula el producto exacto y lo limita al intervalo de int antes de
truncar. Las operaciones y políticas restantes conservan su ruta. Se preserva
el ejemplo original del reporte.

El build incremental terminó 0; compilador SHA-256
`721d0d99577bbba4cf6bd9529c142ea9d53de54b02d51da495472e0dca51e169`.
La nueva regresión ejecuta extremos, signos, productos representables, cero y
wrapping ordinario en O0/O2. Las pruebas registradas de suma, resta y
multiplicación saturadas pasaron 3/3 (`A:/nsc-fixes-evidencia/
int-saturating-mul-ctest-20261112.log`); la suma solo cubre O0. No se acredita
el resto de tipos, operaciones ni plataformas. `CMakeLists.txt` es compartido
con interoperabilidad y requiere revisión al integrar. No se ejecutó batería
completa.
