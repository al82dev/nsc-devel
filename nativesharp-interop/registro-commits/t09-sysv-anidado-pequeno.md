# Registro anidado pequeño en dos piezas SysV

La variante `Other { Inner {int,int}, int }` ya funcionaba en NSC: no requirió un cambio del compilador. La prueba `pruebas-tanda-09-sysv-cpp-copia-anidado-pequeno` conserva su comprobación específica de la llamada indirecta en LLVM, con `i64` e `i32`, así como los resultados observables del registro y la copia.

El control Clang 23.1.0 y NativeSharp pasó las ocho combinaciones de estática/DLL y O0–O3 (`A:/temp/nsc-t09-sysv-cpp-copia-puntero-vjul5alb`). Pasaron además dieciséis consumidores precompilados con y sin encabezado (`A:/temp/nsc-t09-sysv-cpp-copia-puntero-precompilado-f7dtcvvc`). Se trata de una forma concreta en `x86_64-pc-windows-msvc`, no de una certificación general de registros anidados. T09-003 permanece abierto.
