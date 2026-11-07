# T08-015: validación de promociones enteras restantes

La construcción incremental de `nsc` terminó con código 0; registro
`A:/nsc-interop-build/t08-bool-promociones-build.log`. El grupo
`pruebas-tanda-08-record-c/validar.cmd` terminó con código 0; registro
`A:/nsc-interop-build/t08-bool-promociones-record.log`.

El fixture pasó O0–O3 con operandos bool y enteros de 8 y 16 bits, además
de desplazamientos con contadores `uint`, `long` y `ulong` de valor válido.
El verificador comprobó 59 cargas y escrituras de byte y las promociones y
normalizaciones i32/i64. El control negativo de bool nativo permaneció.

La evidencia cubre el arreglo global C `volatile _Bool` en modo MS y los
casos ensayados; no cubre todos los orígenes ni cierra T08-015. La validación
integral posterior sigue pendiente y no se modificó NSLink.
