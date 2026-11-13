# T09: callback C++ variadico SysV con record, ruta viva

## Cambio

NSC admite el record C++ trivial `{double,int}` como argumento opcional de un puntero a funcion variadica `sysv_abi` importado mediante alias C++. La seleccion exige rasgos de copia y la convencion SysV; la sesion Clang localiza el alias calificado, observa el plan y concilia identidad y origen del record; la emision vuelve a verificar el layout. El nombre del plan de prueba sintetico se normaliza porque la llamada es indirecta. T09-003 y T10-003 siguen abiertos.

## Validacion

- Compilacion incremental `build 0`: `A:/temp/t09-cpp-record-replay-build.log`.
- Ocho comparaciones Clang/NativeSharp, biblioteca estatica y DLL O0-O3; el receptor comprueba piezas del record y contador SSE: `A:/temp/nsc-t09-cpp-variadica-sysv-record-indirecta-3iqmgo5m`.
- Rechazos de callback cdecl con record C++ y record no trivial: `A:/temp/nsc-t09-cpp-var-record-indirecta-negativos-a3nxkw1c`.
- Regresion callback C SysV vivo: `A:/temp/nsc-t09-sysv-indirecta-variadica-record-ifppnpv7`; precompilado C sin encabezado: `A:/temp/nsc-t09-sysv-indirecta-record-pre-7lfvy6fe`. Regresion C++ directa: `A:/temp/nsc-t09-cpp-variadica-sysv-record-tj74w2wc`. Prueba ABI Clang: 1/1.
- El consumidor C++ precompilado falla al generar la llamada indirecta: `A:/temp/nsc-t09-cpp-variadica-sysv-record-indirecta-precompilado-kwaqxq8q`; esa ruta permanece pendiente.

## Limites

El receptor usa ensamblador para observar los registros; no usa `va_arg`. Se comprueba una firma, un record trivial y un alias de namespace; no se acredita el resto de firmas, records no triviales, ni la ruta precompilada C++. No se modifica Clang ni NSLink ni se crea versionamiento historico NSMeta.

Actualizacion posterior: el caso tambien pasa precompilado; vease [evidencia y limites](t09-cpp-variadica-sysv-record-indirecta-precompilado.md).


Actualizacion posterior: el callback cdecl con records C++ triviales elegibles tambien funciona; vease [evidencia](t09-cpp-cdecl-record-callback.md).
