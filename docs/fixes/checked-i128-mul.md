# Multiplicación checked de i128

En la revisión `4e5ed96db`, `checked(maximum * 2)` y
`checked(minimum * -1)` continuaban después de desbordarse en O0 y O2,
mientras un producto representable pasaba. Los comandos y resultados previos
están en `A:/nsc-fixes-evidencia/i128-checked-mul-before-20261112.log`.

`applyIntegerBinaryOperation` utiliza el intrinsic de multiplicación con
overflow firmado para `checked` con operandos `i128`; al desbordar emite el
trap de overflow entero existente, código 147. Las demás políticas y tipos no
cambian. La reproducción original de suma del caso 49 permanece intacta.

El build incremental terminó 0; el compilador aislado tuvo SHA-256
`00eaec5a935b1e679770e89db18b865c566e47570405588e0ded83ac56568db5`.
La prueba registrada cubrió en O0/O2 desbordamiento en ambos sentidos,
`minimum * -1`, productos representables, cero y wrapping ordinario; los
operandos checked pasan por una función. Las tres regresiones de suma, resta
y multiplicación pasaron (3/3), como registra
`A:/nsc-fixes-evidencia/i128-checked-mul-ctest-20261112.log`. Estas pruebas no
acreditan todas las combinaciones, otros tipos, plataformas ni conversiones.
`CMakeLists.txt` se comparte con interoperabilidad y requiere revisión al
integrarse. No se ejecutó batería completa.
