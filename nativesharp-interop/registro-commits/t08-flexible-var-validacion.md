# T08-010: resultado de inferencia `var` de miembros flexibles C

La compilación incremental `cmake --build A:\nsc-interop-build\tanda1-ON --target nsc`
terminó con código 0, build `5c1e6c7fa15d8a3e`. El validador
`pruebas-tanda-08-flexible-c/validar-decay.cmd` terminó con código 0. Ejecutó
el caso `var` calificado directamente, con productor y encabezado, y con
metadata sin encabezado, en O0–O3. Los controles de escritura en `const` e
inferencia de un miembro subalineado rechazaron los programas esperados.

El IR de `A:/nsc-interop-build/tanda08-flexible-decay/ProbeVarVolatile.nsproj`
emite `store volatile i32` por el alias local, sin orden atómico `release`.
La comparación previa del modo Microsoft frente a Clang consta en
`procedencia-puntero-volatile-local.md`: por un `volatile int*` alineado Clang
emite `store atomic volatile ... release`. La ejecución aprobada no demuestra
equivalencia de orden. El transporte o definición del modo de acceso sigue
pendiente; esta evidencia no cierra T08-010 ni la tanda 8.
