# T08: ampliación de almacenamiento atómico en operaciones mixtas

El control `_Atomic(int)` con -1 y `/= 2ULL` se compilaba con Clang, pero NSC
lo rechazaba antes de emitir código. Además, el IR de Clang 23.1.0 observado
amplía sin signo el almacenamiento atómico cuando la operación usa un ancho
mayor: para ese ejemplo produce 2147483647. La misma ampliación se observó en
los ejemplos de `short` y `signed char`, incluso con divisores firmados.
Este registro describe el comportamiento de la versión y casos observados; no
afirma que sea una regla general del estándar C. El IR de referencia está en
`A:/nsc-interop-build/tanda08-atomic-int-long-clang.ll`.

Se admite la combinación entera para el global C atómico y se usa la ampliación
observada por Clang en el descenso, conservando el intercambio `seq_cst`. La
construcción incremental y `atomic/validar.cmd` pasaron en O0–O3 para los
ejemplos firmados y sin signo preparados, junto con los controles anteriores.
Registros `A:/nsc-interop-build/tanda08-atomic-int-ulong-build.log` y
`A:/nsc-interop-build/tanda08-atomic-int-ulong-final2.log`. Continúan pendientes
otras combinaciones y el gate completo de T08.
