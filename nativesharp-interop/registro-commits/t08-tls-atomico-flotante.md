# T08: lectura y escritura de TLS C atómico flotante

Los globales `_Thread_local _Atomic(float)` y `_Atomic(double)` se proyectan
cuando Clang acredita tamaño y alineación de 32 y 64 bits respectivamente.
El descenso usa cargas y escrituras enteras atómicas `seq_cst` con bitcast,
conservando los bits y los calificadores `const` y `volatile`. Las operaciones
compuestas flotantes continúan rechazadas y se comprueba ese diagnóstico; no
se presentan como implementación completa. `_Atomic(_Bool)` sigue fuera del
caso proyectado y conserva un control negativo.

La construcción incremental de `nsc` y el control agrupado `atomic/validar.cmd`
pasaron en O0–O3. El control C y NativeSharp comprueban valores iniciales,
escrituras negativas y lectura `const`/`volatile`; se verifica IR por símbolo,
orden y alineación y el rechazo de escritura `const`. Registros:
`A:/nsc-interop-build/tanda08-atomic-float-build.log` y
`A:/nsc-interop-build/tanda08-atomic-float-final.log`. Faltan operaciones,
otros patrones de bits, concurrencia y validación completa de T08.
