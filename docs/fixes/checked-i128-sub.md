# Resta checked de i128

En la revisión `6539c255f`, `checked(minimum - 1)` y
`checked(maximum - (-1))` continuaban después del desbordamiento en O0 y O2.
El control `checked(maximum - 1)` devolvió 42. La reproducción auxiliar,
comandos y resultados están en
`A:/nsc-fixes-evidencia/i128-checked-sub-before-20261112.log`; no modifica el
ejemplo original de suma del caso 49.

La ruta `applyIntegerBinaryOperation` ahora usa el intrinsic de resta con
overflow firmado para `checked` sobre `i128` y el mismo trap de overflow
entero que ya empleaba la suma (código 147). La suma conserva su intrinsic y
la resta ordinaria conserva wrapping. No cambia otras operaciones o tipos.

El build incremental del target `nsc` terminó 0. Con el compilador aislado
SHA-256 `9fc349a0dc1d56d254500407c5fd3285912306667ab956d2056226d52ba3eb3c`,
la regresión de resta cubrió ambos desbordamientos, resultados representables
en los límites y wrapping ordinario en O0 y O2. El runner de suma checked
preexistente pasó también en O0/O2. La prueba registrada
`nsc.compiler.checked-i128-sub` pasó 1/1; evidencia en
`A:/nsc-fixes-evidencia/i128-checked-sub-ctest-20261112.log`. La regresión
dirigida no acredita multiplicación, conversiones ni otros tipos o plataformas.
`CMakeLists.txt` es un archivo compartido con la rama de interoperabilidad y
requiere revisión al integrarse. No se ejecutó batería completa.
