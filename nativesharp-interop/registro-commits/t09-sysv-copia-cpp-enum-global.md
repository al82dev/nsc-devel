# SysV: copia C++ con campo enum global

La función SysV recibe por copia un record C++ trivial con campos `Mode`,
`double` e `int`, y devuelve un record mixto pequeño. Clang clasificó el
argumento como `byval`; después de habilitar el campo, NSC aún rechazaba la
firma del puntero a función. Reproducción:
`A:/temp/nsc-t09-sysv-cpp-copia-relleno-qs27jco8/estatica-O0-ns-ir.log`.

La ruta viva concilia el campo con un enum C++ global de base `int` instalado
con el mismo USR y layout; la validación física lo compara con el `i32` del
plan de Clang. La opción de admitir este campo permanece desactivada en la
conciliación de catálogo precompilado. Los controles del plan rechazan enum sin
USR, calificado por namespace o con otra base.

`pruebas-tanda-09-sysv-cpp-copia-enum/validar.py` pasó sus ocho comparaciones
de Clang 23.1.0 y NativeSharp, estática y DLL en O0–O3, con llamadas directas
y por puntero. Registros:
`A:/temp/nsc-t09-sysv-cpp-copia-relleno-i405325y`. La compilación incremental y
`nsc.cpp-interop.regcall-record-plan` pasaron.

La prueba precompilada falla al generar la metadata del productor con
`required C enum is unavailable`:
`A:/temp/nsc-t09-sysv-cpp-copia-relleno-precompilado-xpnk68nf/estatica-O0-producer-ns.log`.
Ese fallo se resolvió posteriormente; véase
[la validación precompilada](t09-sysv-copia-enum-precompilado.md). Enum en
namespace y otras bases siguen pendientes; T09-003 permanece abierto.
