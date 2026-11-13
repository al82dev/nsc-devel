# T09: record C++ variadico SysV con arreglo fijo

## Cambio

La observacion de sintaxis simple reconoce tambien arreglos fijos unidimensionales de escalares builtin sin calificadores. La reconstruccion existente vuelve a comprobar tamaño, alineacion y cada campo del record antes de obtener el plan ABI. La ruta viva ya funcionaba; el cambio habilita la precompilacion del caso `{int x[3]; double y;}`. T09-003 y T10-003 siguen abiertos.

## Validacion

- Compilacion incremental `build 0`: `A:/temp/t09-cpp-record-replay-build.log`.
- Ocho comparaciones Clang/NativeSharp en estatica y DLL O0-O3, con comprobacion de los tres elementos, el double y la copia: `A:/temp/nsc-t09-cpp-variadica-sysv-record-arreglo-1frjz4s_`.
- Dieciseis consumidores precompilados con y sin encabezado: `A:/temp/nsc-t09-cpp-variadica-sysv-record-arreglo-precompilado-j7ygdxvt`.
- La prueba de observacion pasa y rechaza arreglos multidimensionales y de punteros; el consumidor rechaza la ausencia de sintaxis observada o plan base: `A:/temp/nsc-t09-cpp-var-record-precomp-negativos-njvzoezp`.
- Regresion de namespace precompilado: `A:/temp/nsc-t09-cpp-variadica-sysv-record-namespace-precompilado-sc0ca8be`; regresion C SysV: `A:/temp/nsc-t09-sysv-varargs-kf_b2egu`.

## Limites

La prueba usa un receptor de ensamblador para observar el record en pila, sin `va_arg`. Acredita esa firma y layout; no acredita arreglos multidimensionales, de punteros, de longitud variable, records no triviales u otras formas. Clang y NSLink no se modificaron; se conserva el contrato vigente de NSMeta.