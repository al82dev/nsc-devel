# T08: operandos enteros en operaciones TLS atómicas estrechas

Las operaciones compuestas del global TLS C atómico de 8/16 bits preservan el
tipo del operando derecho entero a través de semántica y HIR. El descenso usa
un tipo de cálculo de 32 o 64 bits, firmado o sin signo según el operando, y
convierte al ancho almacenado únicamente antes del intercambio atómico. La
regla no amplía las conversiones de variables locales ordinarias.

El control comparativo descubrió un detalle de Clang 23.1.0 en Windows: para
`_Atomic(short)` con valor -1 dividido entre `2U`, el IR observado extiende el
almacenamiento sin signo y obtiene 32767. La prueba C y NativeSharp conservan
ese resultado específico; no extrapolan otros casos no probados. Se conserva
el IR observado en `A:/nsc-interop-build/tanda08-atomic-promotion-unsigned.ll`.

La construcción incremental y `atomic/validar.cmd` pasaron en O0–O3 para los
casos preparados con operandos `byte`, `int`, `uint`, `long` y `ulong` y los
controles previos. Registros `A:/nsc-interop-build/tanda08-atomic-narrow-rhs-rebuild.log`
y `A:/nsc-interop-build/tanda08-atomic-narrow-rhs-validar2.log`. La primera
expectativa del control C fue incorrecta y se corrigió tras inspeccionar IR.
Faltan otras combinaciones, concurrencia y validación completa de T08.
