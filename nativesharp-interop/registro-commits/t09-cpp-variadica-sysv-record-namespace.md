# T09: record C++ variadico SysV en namespace

## Cambio

La ruta variadica SysV concilia el namespace nombrado y el USR del record C++ trivial al observar su origen, pedir el plan ABI y reconstruirlo desde el catalogo. La observacion de sintaxis simple admite namespaces nombrados y sigue descartando contextos anonimos y records anidados. El fixture declara `Types::T09VarValue {double,int}` y un homonimo `Other::T09VarValue {long long,int}` para comprobar que la busqueda calificada encuentra el tipo correcto. T09-003 y T10-003 permanecen abiertos.

## Validacion

- Compilacion incremental `build 0`: `A:/temp/t09-cpp-record-replay-build.log`.
- Ocho comparaciones Clang/NativeSharp, estatica y DLL O0-O3: `A:/temp/nsc-t09-cpp-variadica-sysv-record-namespace-ree6vae0`.
- Dieciseis consumidores precompilados con y sin encabezado: `A:/temp/nsc-t09-cpp-variadica-sysv-record-namespace-precompilado-hn2w5_p1`.
- Rechazo sin observacion o sin plan base: `A:/temp/nsc-t09-cpp-var-record-precomp-negativos-ntxtfx46`. Pruebas unitarias de observacion de valor y plan Clang: 1/1 cada una.
- Regresion del record global precompilado: `A:/temp/nsc-t09-cpp-variadica-sysv-record-precompilado-icnyg3tr`; regresion C SysV: `A:/temp/nsc-t09-sysv-varargs-imxditz2`.

## Limites

El receptor de prueba observa registros en ensamblador, sin `va_arg`. Se acredita el record y la firma indicados; namespaces anonimos, records anidados, atributos, records no triviales y otras formas siguen pendientes. No se modifica Clang ni NSLink ni se introduce versionamiento historico de NSMeta.