# T09/T10: enum C++ en plantilla y llamada variadica cdecl

## Cambio

Una plantilla precompilada conserva el enum C++ que nombra en su cuerpo, incluso cuando no aparece en su firma. La captura usa la busqueda calificada de tipos importados y el catalogo vigente; la verificacion de la plantilla recibe tambien el nombre calificado del enum. Al recuperar un enum C++ completo con representacion conocida, NSC permite declararlo como valor local sin tratarlo como un objeto que necesita lifetime. Esto resuelve una dependencia no lineal de T10-002/T10-003 para T09-003; los items siguen abiertos.

## Validacion

- Compilacion incremental `build 0`: `A:/temp/t09-cpp-record-replay-build.log`.
- Enum global no scoped `unsigned short`, valor 65535 promovido a `int` y consumido por `va_arg`: ocho comparaciones Clang/NativeSharp `A:/temp/nsc-t09-cpp-variadica-cdecl-enum-5bd2ud8g`; 16 consumidores precompilados `A:/temp/nsc-t09-cpp-variadica-cdecl-enum-precompilado-90jq3dod`.
- Mismo caso en `T09Cpp::Mode`, con otro `Mode` homonimo en `Other`: ocho comparaciones `A:/temp/nsc-t09-cpp-variadica-cdecl-enum-namespace-i57a_u93` y 16 consumidores con y sin encabezado `A:/temp/nsc-t09-cpp-variadica-cdecl-enum-namespace-precompilado-0a3l9iak`.
- Regresiones: record SysV con campo enum precompilado `A:/temp/nsc-t09-sysv-cpp-copia-relleno-precompilado-cit89n4_`; llamadas variadicas C SysV `A:/temp/nsc-t09-sysv-varargs-ft25odus`.

## Limites

La prueba acredita el enum no scoped y base `unsigned short` indicados, en Windows x64, no todos los enums ni promociones. No modifica Clang ni NSLink y no introduce versionamiento historico de NSMeta.