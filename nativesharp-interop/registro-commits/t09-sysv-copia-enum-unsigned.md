# SysV: campo enum C++ unsigned int

Después de importar el enum y su campo, la firma SysV del puntero todavía se
rechazaba: `A:/temp/nsc-t09-sysv-cpp-copia-relleno-bbyrqf0w/estatica-O0-ns-ir.log`.
La conciliación compara ahora la base `unsigned int`, su identidad y su layout
con el `i32` físico, sin confundirla con la base `int`.

`pruebas-tanda-09-sysv-cpp-copia-enum-unsigned/validar.py` pasó las ocho
comparaciones Clang 23.1.0/NativeSharp, estática y DLL O0–O3, con llamadas
directa y por puntero y el valor `0xFFFFFFFF`:
`A:/temp/nsc-t09-sysv-cpp-copia-relleno-z_i2cvv8`. El control del plan ABI
pasó, incluyendo rechazo de tipo y representación inconsistentes.

El productor precompilado falla por enum no disponible en el catálogo:
`A:/temp/nsc-t09-sysv-cpp-copia-relleno-precompilado-ehguqk2_/estatica-O0-producer-ns.log`.
Ese fallo se resolvió después; véase [la validación precompilada](t09-sysv-enum-unsigned-precompilado.md).
Otras bases y enums scoped siguen pendientes. T09-003 permanece abierto.
