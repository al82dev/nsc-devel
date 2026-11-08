# Conversiones saturadas de int a short y byte

Base de la campana 152654e5a0e2d84d4c2f245ea7590488f0497430. Parte de COMP-01,
A:/native-sharp-reference/REPORTE-PARA-WORKER-COMPILADOR-SDK.md.

## Alcance
Corregir las dos conversiones del ejemplo 48. No se modifican A:/nsc-devel, los ejemplos
originales, C:/native-sharp, LLVM/Clang, Daikon ni la interoperabilidad C/C++; no se usan
sus builds ni se hace merge, rebase, push o despliegue. Builds, temporales y evidencias en A.
Las decisiones de lenguaje o API pendientes requieren decision antes de cambiar comportamiento.
El baseline y la bateria completa pertinentes siguen pendientes.

## Causa y cambio
MIR muestra conversiones int a short y a byte bajo saturating. convertScalar truncaba sin
consultar la politica. Para esas dos conversiones y solo bajo saturating se limita el valor
al rango de destino antes de truncarlo. Las conversiones ordinarias conservan su camino.

## Pruebas y limites
nsc.compiler.saturating-narrow fallo antes y paso despues, cubriendo ambos limites, valores
interiores y una conversion ordinaria. Compilo y ejecuto en O0 y O2. La reproduccion
historica copiada muestra ahora Int policy=2147483647 Audio cast=32767 Color cast=255.
La regresion de suma saturada paso nuevamente. Evidencia en A:/nsc-fixes-evidencia/
baseline-sat-narrow.log, fixed-sat-narrow.log, sat-narrow-o2/ y cases/comp-01/fixed-result.json.

clang-tidy con clang-analyzer-* no informo errores y produjo nueve avisos de padding en
cabeceras no modificadas; no hay linea base comparable del analizador. No se afirman
verificadas conversiones entre otros tipos ni la bateria completa.
