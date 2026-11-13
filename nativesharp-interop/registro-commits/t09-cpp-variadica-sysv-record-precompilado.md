# T09: record C++ variadico SysV precompilado

## Cambio

El consumidor reconstruye con Clang una declaracion C++ equivalente para records globales triviales cuya sintaxis simple fue observada en el catalogo vigente. Comprueba campos, disposicion, modulo, USR, simbolo, firma y plan base antes de solicitar el plan variadico completo y emitir la llamada. Si falta la observacion o el plan base, rechaza. El proveedor C permanece separado. Este es un avance parcial de T09-003 y una dependencia de T10-003, no cierra ambos items.

## Validacion

- Compilacion incremental `build 0`, `A:/temp/t09-cpp-record-replay-build.log`.
- Dieciseis consumidores precompilados con y sin encabezado, en estatica y DLL O0-O3, para el record `{double,int}`: `A:/temp/nsc-t09-cpp-variadica-sysv-record-precompilado-3l9kaupk`.
- Consumidor sin encabezado rechaza el catalogo al retirar la observacion de sintaxis o el plan base: `A:/temp/nsc-t09-cpp-var-record-precomp-negativos-sb9swi4h`.
- Ocho comparaciones de la ruta viva C++: `A:/temp/nsc-t09-cpp-variadica-sysv-record-6k_e3bm6`; regresion C SysV, incluidas rutas precompiladas: `A:/temp/nsc-t09-sysv-varargs-uctmzpx5`; `nsc.cpp-interop.clang-abi-probe`: 1/1.

## Limites

La ejecucion comprueba esta firma y el layout `{double,int}` mediante un receptor de registros en ensamblador, sin `va_arg`. La reconstruccion exige un record global simple, elegible y con campos reconstruibles, una firma fija builtin y una identidad de funcion no ambigua; registros con atributos, bases, tipos no triviales, otros layouts y firmas permanecen sin certificacion. No se modifica Clang, NSLink ni se introduce versionamiento historico de NSMeta.