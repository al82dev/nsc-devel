# T09: enum class C++ variádico SysV

## Cambio

En Windows x64, la muestra `enum class Mode : unsigned short` se enviaba con `i32` aunque Clang 23.1.0 usa `i16 zeroext`; para `short` usa `i16 signext`. Se conserva la identidad del enum scoped en las llamadas SysV directas y por callback. La sesión Clang observa el plan ABI del tipo real; el emisor distingue enums de records y aplica los atributos físicos del plan. En consumidores precompilados se reconstruye el enum a partir del catálogo vigente y se cotejan identidad, base, signo, tamaño y alineación antes de pedir un nuevo plan a Clang. No se altera el contrato de NSMeta.

## Pruebas

- Compilación incremental: `A:/temp/t09-cpp-record-replay-build.log`, código 0. Pruebas internas ABI, observación de record y plan de función: `A:/temp/t09-scoped-enum-unit-tests.log`, las tres con código 0.
- `unsigned short`: ocho comparaciones Clang/NSC de llamada directa y ocho por callback, con aserciones `i16 zeroext` en IR, estática y DLL O0–O3: `A:/temp/nsc-t09-cpp-variadica-sysv-enum-scoped-yfqrfrlp` y `A:/temp/nsc-t09-cpp-variadica-sysv-enum-scoped-indirecta-4tq28btx`.
- Consumidores precompilados, con y sin encabezado: 16 ejecuciones por ruta, `A:/temp/nsc-t09-cpp-variadica-sysv-enum-scoped-precompilado-r85vn6yk` y `A:/temp/nsc-t09-cpp-variadica-sysv-enum-scoped-indirecta-precompilado-j4eem9w0`.
- `short` con valor negativo, aserción `i16 signext`, ejecución y precompilación en ambas rutas: `A:/temp/nsc-t09-sysv-enum-signo-hmw263z7`. El IR O0 de Clang está en `A:/temp/t09-sysv-enum-signed-control.ll`.
- Regresión de records SysV precompilados directos e indirectos: `A:/temp/nsc-t09-cpp-variadica-sysv-record-precompilado-0j7z6trl` y `A:/temp/nsc-t09-cpp-variadica-sysv-record-indirecta-precompilado-3adllowv`. Regresiones cdecl scoped y no scoped: `A:/temp/nsc-t09-cpp-variadica-cdecl-enum-scoped-sd3h85d2` y `A:/temp/nsc-t09-cpp-variadica-cdecl-enum-precompilado-y84srqch`.

## Límites

El receptor SysV es ensamblador que comprueba registros; no acredita `va_arg` SysV en Windows. Solo se probaron estas bases y firmas, no las demás combinaciones ni otras plataformas. El IR comprobado es el de la ruta viva; la ruta precompilada se validó por ejecución. Las declaraciones de origen ambiguo se rechazan. T09-003, T10-002 y T10-003 siguen abiertos.
