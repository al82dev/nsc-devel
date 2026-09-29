# Tanda 18: paralelismo de las pruebas self-host largas

## Objetivo

Reducir el tiempo de pared del baseline sin cambiar la semantica de ninguna prueba y sin ejecutarlo durante esta tanda. El cambio se concentra en las cuatro pruebas self-host que dominaron el tiempo de la corrida cancelada.

## Diagnostico

CTest ya se invocaba con `-j 12`, pero las pruebas SH41, SH42, SH43 y SH44 tenian `RUN_SERIAL TRUE`. Ademas, SH42, SH43 y SH44 formaban una cadena artificial aunque las tres solo leen los ejecutables producidos por SH41 y escriben en directorios separados.

Los tiempos observados en la corrida `20260927-205227-c82de6e8` fueron:

- SH41 `self-hosted-generation`: 1035.12 s.
- SH42 `functional-equivalence`: 272.74 s.
- SH43 `repeated-fixed-point`: 848.79 s.
- SH44 `cross-machine-determinism`: 1009.87 s.

La cadena obligaba a pagar 3166.52 s antes de continuar con SH45, aunque SH42, SH43 y SH44 no dependen entre si.

## Cambios

- SH41 deja de bloquear globalmente a CTest y declara cuatro procesadores.
- SH42, SH43 y SH44 dependen directamente de SH41, dejan de ser globalmente seriales y reservan cuatro procesadores cada una.
- Con `ctest -j 12`, las tres ramas posteriores a SH41 pueden ocupar exactamente los doce cupos disponibles.
- SH45 conserva su ejecucion serial y ahora espera explicitamente la terminacion correcta de las tres ramas.
- Se asignaron costos basados en la corrida observada para que CTest priorice temprano las pruebas de mayor duracion cuando esten listas.

## Archivos modificados

- `CMakeLists.txt`: corrige dependencias y propiedades de planificacion para SH41-SH45.

## Archivo nuevo

- `docs/2.0/development/namespace-functions/TANDA-18-PARALELISMO-SELFHOST-CTEST.md`: documenta el diagnostico, la topologia nueva, el beneficio esperado y la verificacion de la tanda.

No se agregaron ejecutables, scripts ni directorios de construccion.

## Mejora esperada

La porcion SH41-SH44 de la ruta critica pasa, idealmente, de la suma de las cuatro duraciones a SH41 mas la mayor de las tres ramas. Con los tiempos observados, la reduccion teorica es de 1121.53 s, aproximadamente 18 minutos y 42 segundos.

Ademas, mientras SH41 ocupa cuatro cupos, CTest puede utilizar los ocho restantes para pruebas ordinarias que no exijan ejecucion globalmente serial. El resultado real dependera de la contencion de CPU, memoria y disco durante el baseline externo.

## Seguridad del cambio

- SH42, SH43 y SH44 solo consumen los productos `nsc2-*` de `selfhost-sh41`.
- Cada rama usa sus propios directorios de salida e insumos: `selfhost-sh42`, `selfhost-sh43[-inputs]` y `selfhost-sh44[-inputs]`.
- SH45 no comienza hasta que las tres ramas hayan terminado.
- Los contratos y comandos internos de cada prueba permanecen intactos.

## Verificacion

- Construccion incremental exclusiva del target `nsc`: correcta.
- Regeneracion de CMake con pruebas habilitadas: correcta.
- Inspeccion de `ctest --show-only=json-v1`: SH41-SH44 publican `PROCESSORS=4`, costos y dependencias esperadas.
- Inspeccion del grafo: SH42, SH43 y SH44 dependen de SH41; SH45 depende de las tres.
- No se ejecuto ninguna prueba, baseline ni matriz de optimizaciones.
- Los archivos `error*.log` de la corrida cancelada no se modificaron ni se incluyeron.
