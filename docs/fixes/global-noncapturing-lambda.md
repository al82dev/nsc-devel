# Lambda sin capturas en funcion global

Base de la campana 152654e5a0e2d84d4c2f245ea7590488f0497430. Caso del ejemplo 69 del
reporte A:/native-sharp-reference/REPORTE-PARA-WORKER-COMPILADOR-SDK.md.

## Alcance
Corregir la lambda sin capturas de Main global. No se modifican A:/nsc-devel, los ejemplos
originales, C:/native-sharp, LLVM/Clang, Daikon ni interoperabilidad C/C++; no se usan sus
builds ni se hacen merge, rebase, push o despliegue. Builds, temporales y evidencias en A.
Las decisiones de lenguaje y API no se cambian sin decision. El baseline y la bateria completa
pertinentes permanecen pendientes.

## Causa y correccion
El compilador representa el owner de una funcion global como el numero de miembros mas el
ordinal de la funcion de namespace. buildLambdaFunction aceptaba solo indices de miembros y
rechazaba ese owner con Stage 43 lambda owner member ordinal is invalid. Ahora acepta un owner
global que realmente exista y cuya lambda no tenga capturas, y usa el ordinal de tipo ausente
que ya utilizan las funciones de namespace. El camino de metodos sigue usando su tipo original.
Las lambdas globales con capturas no se han corregido ni verificado con este cambio.

## Pruebas
La prueba nsc.compiler.global-lambda ejecuta una lambda de dos parametros y comprueba 3.
Fallo antes con el diagnostico anterior y paso despues. La fuente historica copiada compilo y
retorno 3, su valor esperado. La prueba existente nsc.stage43.llvm y las regresiones
nsc.sdk.dictionary-string-update y nsc.compiler.decimal-generic-array pasaron con el compilador
actualizado. Evidencias en A:/nsc-fixes-evidencia/baseline-global-lambda.log,
fixed-global-lambda.log, stage43-llvm-after-global-lambda.log y cases/69-lambda/.

La bateria general y el analisis estatico integral siguen pendientes. No se infiere soporte
de todas las formas de lambda a partir de estas pruebas.
