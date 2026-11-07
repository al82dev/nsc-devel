# T08: validación agrupada de TLS atómico

La construcción incremental de `nsc` pasó después de incorporar `/=` y `%=`.
El control `atomic/validar.cmd` pasó en C y NativeSharp O0–O3: compara cociente
y residuo con operandos negativos y verifica multiplicación, desplazamiento,
división y residuo sobre un TLS `volatile`. El verificador de IR exige cuatro
intercambios para ese objeto y, en NativeSharp, que estén marcados `volatile`.
Un hilo C cambia el signo mientras el hilo NS divide por uno sobre el mismo
objeto TLS. Este control concurrente no demuestra todas las intercalaciones.

Los registros son `A:/nsc-interop-build/tanda08-atomic-div-group-build.log` y
`A:/nsc-interop-build/tanda08-atomic-div-group-validar.log`. Continúan pendientes
la comprobación de divisor cero, otros tipos y operaciones, y la validación
integral de T08. No se modificó NSLink.
