# T08: operaciones compuestas TLS C atómicas flotantes

El descenso de `_Atomic(float)` y `_Atomic(double)` utiliza el patrón de bits
como almacenamiento entero, calcula el valor flotante y reintenta `cmpxchg`
secuencialmente consistente si cambia el objeto. La dirección y el operando
se evalúan antes del bucle; el intercambio conserva `volatile`. El cálculo
utiliza las reglas y el modo de punto flotante existentes de NativeSharp.

La construcción incremental de `nsc` y `atomic/validar.cmd` pasaron en O0–O3.
El control C y NativeSharp comprueban suma, resta, multiplicación y división
con operandos preparados para `float` y `volatile double`. El IR NativeSharp
debe contener intercambios atómicos por símbolo, incluidos cuatro `volatile`
alineados a 8 para el `double`. Registros:
`A:/nsc-interop-build/tanda08-atomic-float-compound-build.log` y
`A:/nsc-interop-build/tanda08-atomic-float-compound-final.log`.
Faltan patrones especiales, concurrencia y el gate completo de T08.
