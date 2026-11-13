# T09: record C++ opcional en funcion variadica SysV, ruta viva

## Cambio

Para una funcion libre variadica `sysv_abi`, NSC admite un record C++ global completo, trivial y de layout estandar como argumento opcional cuando puede conciliar origen Clang, identidad, dimensiones y plan ABI. La emision usa las piezas del plan tras comprobar el layout importado. La clasificacion se limita al candidato de namespace SysV; otras convenciones, records no triviales y records sin plan siguen rechazados. El cambio es una dependencia parcial de T10-003 y no cierra T09-003.

## Validacion

- Compilacion incremental: `build 0`, `A:/temp/t09-regcall-reconcile-build.log`.
- Record C++ `{double,int}` por valor, conservacion del original, simbolo y piezas ABI: ocho comparaciones Clang/NativeSharp en biblioteca estatica y DLL O0-O3, `A:/temp/nsc-t09-cpp-variadica-sysv-record-my51xs7f`.
- Rechazos con biblioteca existente de la misma forma en `cdecl` y de un record con destructor no trivial: `A:/temp/nsc-t09-cpp-var-record-negativos-akzb_jja`.
- `nsc.cpp-interop.clang-abi-probe`: 1/1; regresion C variadica SysV, incluida precompilacion C: `A:/temp/nsc-t09-sysv-varargs-82nks2xt`.
- La prueba precompilada C++ sigue fallando en el primer consumidor con `sysv_abi variadic argument type is not yet supported`: `A:/temp/nsc-t09-cpp-variadica-sysv-record-precompilado-hpk_zugv`. No se considera implementada esa ruta.

## Limites

El receptor de la prueba observa registros por ensamblador, no usa `va_arg`. Solo se demuestra el layout indicado en ruta viva; records no triviales, records en namespaces, otras firmas y precompilacion siguen pendientes. La prueba admite `call` o `invoke` LLVM y exige la convencion y firma variadica. El contrato NSMeta no se versiona ni se modifica.

Actualizacion posterior: la muestra tambien funciona precompilada; vease [reconstruccion y limites](t09-cpp-variadica-sysv-record-precompilado.md).


Actualizacion posterior: records C++ triviales elegibles tambien pasan por funciones cdecl variadicas directas; vease [evidencia](t09-cpp-variadica-cdecl-record.md).
