# T09: C++ variadica SysV con argumentos en pila

## Alcance

Se amplia la evidencia de la funcion libre C++ variadica `sysv_abi` mediante un fixture separado: parametro fijo `int`, siete enteros y nueve `double` opcionales. Un receptor de prueba en ensamblador `naked` comprueba el parametro fijo, los siete enteros en sus registros o en pila, el primer y octavo flotante en registro, el noveno en pila y el contador de registros SSE. Tambien se comprueba la seleccion de una sobrecarga fija.

## Validacion

- Ocho comparaciones del llamador Clang C++ y NativeSharp, enlace estatico y DLL en O0 a O3: `A:/temp/nsc-t09-cpp-variadica-sysv-pila-mld3_m29`.
- Dieciseis consumidores precompilados, con y sin encabezado, estatico y DLL en O0 a O3: `A:/temp/nsc-t09-cpp-variadica-sysv-pila-precompilado-_46ycta_`.
- No hubo cambios al compilador ni fue necesaria una reconstruccion.

## Limites

El receptor observa registros y pila para esta firma concreta, no usa `va_arg`. No se comprobaron los valores intermedios de los registros flotantes ni otros ordenes de argumentos, records u otras convenciones. Esta evidencia no cierra T09-003 ni T10-003 y no certifica interoperabilidad general.