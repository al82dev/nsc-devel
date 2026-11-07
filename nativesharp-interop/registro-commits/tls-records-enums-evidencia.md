# Evidencia de records y enums TLS C

Con NSC `7dd91639096aa9fa`, la construcción incremental y la suite de
sesiones pasaron. El control C y NativeSharp leyeron y escribieron un struct
TLS, un puntero a él, un arreglo de structs, un enum y un puntero al enum;
comprobaron aislamiento del segundo hilo. El conjunto pasó O0–O3 en estático
con LLD y en DLL con NSLink sin modificaciones.

También pasaron las regresiones de punteros calificados y arreglos TLS
escalares, incluidos los controles `const` y `volatile`. Esta evidencia cubre
las declaraciones del fixture, no todos los records o enums. Las DLL mínimas
y la validación integral de tanda 8 siguen pendientes.
