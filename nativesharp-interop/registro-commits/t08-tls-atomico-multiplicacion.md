# T08: multiplicación compuesta de TLS C atómico

El global C `_Thread_local _Atomic(int)` rechazaba `*=`. El descenso para el
tipo y alineación ya admitidos realiza una carga atómica y reintenta mediante
`cmpxchg` hasta intercambiar el valor observado; ambos órdenes del intercambio
son secuencialmente consistentes. El operando derecho se evalúa antes del
bucle, y `volatile` se conserva en carga e intercambio. No se habilitan otras
operaciones previamente rechazadas; el control negativo utiliza ahora `/=`.

La construcción incremental de `nsc` pasó. `atomic/validar.cmd` pasó el control
C y los ejecutables NativeSharp O0–O3; verifica el resultado, una sola llamada
al operando derecho, instrucciones `cmpxchg` del global con ambos órdenes
`seq_cst`, y dos hilos multiplicando el mismo objeto TLS mientras vive su
propietario. Los registros están en `A:/nsc-interop-build/tanda08-atomic-mul-build.log`
y `A:/nsc-interop-build/tanda08-atomic-mul-contencion.log`. La prueba no demuestra
las demás operaciones ni la validación integral de T08.
