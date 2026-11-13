# T09: record C++ en funcion cdecl variadica

## Cambio

La seleccion de funciones importadas `cdecl` admite por valor records C++ completos con identidad y layout y operaciones de copia triviales elegibles. La ruta Win64 existente transporta el record. El productor C++ de prueba recupera el valor con `__builtin_va_arg`, y la prueba comprueba que el objeto original no cambia. T09-003 y T10-003 permanecen abiertos.

## Validacion

- Compilacion incremental `build 0`: `A:/temp/t09-cpp-record-replay-build.log`.
- Record `{double,int}` de 16 bytes, ocho comparaciones Clang/NativeSharp O0-O3 con estatica y DLL: `A:/temp/nsc-t09-cpp-variadica-cdecl-record-wjq36i4_`; 16 consumidores precompilados con y sin encabezado: `A:/temp/nsc-t09-cpp-variadica-cdecl-record-precompilado-7fl1k9x9`.
- Records compactos de 1, 2, 4 y 8 bytes, cada uno comparado con Clang y precompilado en las mismas configuraciones: `A:/temp/nsc-t09-cpp-cdecl-record-compactos-pzq1mjv9`.
- Se conserva el rechazo de cdecl con destructor no trivial o copia eliminada y SysV con destructor no trivial: `A:/temp/nsc-t09-cpp-var-record-negativos-jmf6gken`. La prueba previa que esperaba rechazo de cdecl trivial cambio su entrada a un tipo no trivial, pues la forma trivial ahora funciona.
- Regresiones: cdecl escalar C++ `A:/temp/nsc-t09-cpp-variadica-cdecl-idokscix`, callback SysV C++ `A:/temp/nsc-t09-cpp-variadica-sysv-record-indirecta-f6gxunkx` y C SysV `A:/temp/nsc-t09-sysv-varargs-3gu3pan_`.

## Limites

Se acreditan estos tamaños y layouts, no todos los records C++ ni callbacks cdecl con records. Se rechazan las operaciones de copia no elegibles. Clang y NSLink no se modificaron; el contrato de NSMeta sigue siendo unico.

Actualizacion posterior: el callback cdecl con records C++ triviales elegibles tambien funciona; vease [evidencia](t09-cpp-cdecl-record-callback.md).
