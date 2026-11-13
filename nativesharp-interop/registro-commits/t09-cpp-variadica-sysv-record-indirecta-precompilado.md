# T09: callback C++ variadico SysV con record precompilado

## Cambio

El proveedor del callback reconstruye el alias y los records C++ simples desde el catalogo vigente, usando una sesion Clang C++. Se cotejan identidad y firma del alias, origen y rasgos del record, campos y layout, antes de observar el plan de llamada indirecta. La ruta C conserva su reconstruccion anterior. La ausencia de evidencia impide la emision. Este es un avance parcial de T09-003 y dependencia de T10-003; no cierra los items.

## Validacion

- Compilacion incremental `build 0`: `A:/temp/t09-cpp-record-replay-build.log`.
- Dieciseis consumidores precompilados con y sin encabezado, estatica y DLL O0-O3: `A:/temp/nsc-t09-cpp-variadica-sysv-record-indirecta-precompilado-sqttphou`.
- Sin observacion de sintaxis el consumidor rechaza el plan; al retirar el alias el consumidor rechaza la declaracion: `A:/temp/nsc-t09-cpp-var-record-indirecta-precomp-negativos-_peefa6w`.
- Regresion de callback C precompilado sin encabezado: `A:/temp/nsc-t09-sysv-indirecta-record-pre-7y2dz13x`; prueba ABI Clang 1/1. La ruta viva C++ se acredito en el registro anterior.

## Limites

Se acredita la firma callback `int(int,...)` con record trivial `{double,int}`, mediante un receptor de registros en ensamblador, sin `va_arg`. No se certifican todos los tipos, records no triviales o mezclas C/C++ por valor. La eliminacion del alias se detecta al analizar el consumidor, no mediante un diagnostico ABI especifico. No se modificaron Clang ni NSLink ni se creo versionamiento historico de NSMeta.