# SysV: campo enum C++ short

Después de importar el tipo `short`, NSC todavía rechazaba la firma del
puntero SysV: `A:/temp/nsc-t09-sysv-cpp-copia-relleno-loj8zx5w/estatica-O0-ns-ir.log`.
La conciliación verifica ahora la identidad y el layout del enum de 16 bits y
los compara con el `i16` físico observado por Clang.

`pruebas-tanda-09-sysv-cpp-copia-enum-short/validar.py` pasó ocho
comparaciones Clang 23.1.0/NativeSharp, en estática y DLL O0–O3, directas y
por puntero, usando el valor −32768:
`A:/temp/nsc-t09-sysv-cpp-copia-relleno-cgn1005a`. El control del plan pasó,
incluido el rechazo de una base con tamaño inconsistente.

El productor precompilado todavía falla con `required C enum is unavailable`:
`A:/temp/nsc-t09-sysv-cpp-copia-relleno-precompilado-uugqstdf/estatica-O0-producer-ns.log`.
La reproducción queda en `validar-precompilado-pendiente.py`. T09-003 sigue
abierto y no se acreditan otras bases ni la precompilación.
