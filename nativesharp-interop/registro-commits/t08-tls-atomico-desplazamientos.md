# T08: desplazamientos sobre TLS C atómico

El descenso de `_Thread_local _Atomic(int)` rechazaba `<<=` y `>>=`. Para el
caso firmado de 32 bits ya admitido se usan operaciones `shl` y `ashr` dentro
del mismo bucle `cmpxchg` secuencialmente consistente que la multiplicación.
El operando derecho se calcula antes del bucle. No se extiende el soporte a
otros tipos ni se determina el comportamiento de operandos fuera de rango.

La construcción incremental de `nsc` y `atomic/validar.cmd` pasaron. Los
controles comparan C y NativeSharp O0–O3 para desplazamientos positivos y uno
negativo a la derecha, comprueban resultado y exigen intercambios atómicos en
el IR. Registros: `A:/nsc-interop-build/tanda08-atomic-shift-build.log` y
`A:/nsc-interop-build/tanda08-atomic-shift-validar.log`. T08 sigue abierta.
