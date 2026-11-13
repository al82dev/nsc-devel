# T09: callback C++ cdecl variadico con record

## Cambio

Una llamada indirecta cdecl variadica acepta records C++ completos con identidad, layout y operaciones de copia triviales elegibles. El receptor C++ los recupera mediante `__builtin_va_arg`; la llamada usa el transporte Win64 existente y conserva el objeto original. T09-003 y T10-003 siguen abiertos.

## Validacion

- Compilacion incremental `build 0`: `A:/temp/t09-cpp-record-replay-build.log`.
- Record `{double,int}` de 16 bytes: ocho comparaciones Clang/NativeSharp, estatica y DLL O0-O3, `A:/temp/nsc-t09-cpp-variadica-cdecl-record-indirecta-zonccu0r`; 16 consumidores precompilados con y sin encabezado, `A:/temp/nsc-t09-cpp-variadica-cdecl-record-indirecta-precompilado-yw_gkxri`.
- Records compactos de 1, 2, 4 y 8 bytes, cada uno en ruta viva y precompilada con las mismas configuraciones: `A:/temp/nsc-t09-cpp-cdecl-record-indirecta-compactos-vaq18yr2`.
- Se conserva el rechazo de cdecl con destructor no trivial o copia eliminada y de SysV con destructor no trivial: `A:/temp/nsc-t09-cpp-var-record-indirecta-negativos-znm5edxa`. La prueba previa de rechazo cdecl cambio su entrada a un record no trivial, porque el record trivial ahora funciona.
- Regresiones: callback C SysV precompilado `A:/temp/nsc-t09-sysv-indirecta-record-pre-7sumjgn4` y llamada cdecl directa C++ `A:/temp/nsc-t09-cpp-variadica-cdecl-record-gpky1y3j`.

## Limites

Se acreditan las firmas y layouts indicados, no todos los records o convenciones C++. Los records con operaciones de copia no elegibles siguen rechazados. Clang, NSLink y el contrato unico NSMeta se conservan.