# T08: división y residuo de TLS C atómico

Se extiende el bucle de comparación e intercambio del global C atómico `int`
para `/=` y `%=`. El cálculo usa el descenso entero existente de NativeSharp,
incluida su comprobación de divisor cero; el bloque de reintento usa el bloque
real posterior a esa comprobación como predecesor del valor observado. El
operando derecho se evalúa antes del bucle. El control negativo se traslada a
un TLS `_Atomic(long long)`, que todavía no está proyectado.

Estado: **implementado, sin validar**. El fixture añade comparación con Clang
para cociente y residuo negativos. Queda pendiente la construcción incremental
agrupada y la ejecución de los controles, incluidos divisor cero y contención.
No se ha cerrado T08.
