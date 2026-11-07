# T08-015: validación de `_Bool` C compuesto con enteros anchos

La construcción incremental de `nsc` y el grupo
`pruebas-tanda-08-record-c/validar.cmd` terminaron con código 0. Registros:
`A:/nsc-interop-build/t08-bool-enteros-anchos-build.log` y
`A:/nsc-interop-build/t08-bool-enteros-anchos-record.log`.

El fixture pasó O0–O3 con `uint`, `long` y `ulong` en las operaciones
aritméticas y bit a bit preparadas. Incluyó valores por encima de 32 bits,
resultados negativos y división y residuo con y sin signo. El verificador IR
confirmó 48 cargas acquire y escrituras release de byte, promociones y
normalizaciones i32/i64 e instrucciones de división y residuo de ambos signos.
El control negativo de bool nativo permaneció en el grupo.

Esto cubre el arreglo global C `volatile _Bool` en modo MS y los casos
ensayados, no otros orígenes ni desplazamientos con operandos anchos. T08-015
y la validación integral posterior siguen pendientes; no se modificó NSLink.
