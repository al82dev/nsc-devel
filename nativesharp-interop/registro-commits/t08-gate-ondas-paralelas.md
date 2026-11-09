# T08-GATE-02: prueba de topología de ondas paralelas

Fecha: 2026-11-09.

## Resultado

Resuelto con una corrección en el compilador. `nsc_project_method_body_model_tests` pasa sus 120 comprobaciones;
antes fallaba en una.

## Síntoma

La comprobación «wave topology records the requested job budget and actual parallel waves» fallaba de forma
reproducible, también aislada (`t08-prueba-cuerpos-enlace-precompilado.md`, `t08-alineacion-aritmetica-alias-c.md`).
Exige que la compilación con un job (`-j1`) no registre olas paralelas, y que las de 2 y 12 jobs sí las registren.

## Causa

`ProjectCompilation` siempre presta su pool compartido al análisis de cuerpos (`methodBodyOptions.workerPool`),
también con un solo job. `ProjectCallableWaveScheduler` consideraba paralela cualquier ola de dos o más callables si
había pool, sin mirar cuántos trabajadores tenía. Con `-j1`, cada ola se mandaba al único trabajador y el hilo
llamador lo esperaba: trabajo serial contado como ola paralela, con un cambio de hilo inútil.

## Corrección

`ProjectCallableWaveScheduler.cpp` gana `hasParallelWorkers(pool)`, verdadero solo si el pool tiene más de un
trabajador. Lo usan `usesParallelWave` (estadística) y `runExclusive` (ejecución), así que las dos siguen de
acuerdo. Con un trabajador, la ola se analiza en el hilo llamador, como cuando no hay pool. Con dos o más, nada
cambia.

## Pruebas

- `nsc_project_method_body_model_tests`: 120 de 120 (antes, 119 de 120).
- `nsc_project_callable_wave_scheduler_tests`: 12 de 12.
- La validación integral de la tanda 8 se ejecutó con este compilador (`t08-validacion-integral-20261109.md`).

## Observación: `nsc_project_oop_lowering_tests` completo

La ejecución completa, sin el foco `mir-cache-restore-depth` que usa el gate, falla 26 de 660 comprobaciones.
Ejemplos:

- byte-identidad O0-g, por una biblioteca de runtime buscada en `C:`;
- agregados, `With`/spread;
- `EnumToString`;
- asignadores.

Esos mismos 26 fallos aparecen, idénticos, en dos líneas base:

- sin esta corrección;
- además sin la corrección de T08-019 (`35aedbaea~1`).

Son previos y ajenos a la tanda 8 de interop. Los registros están en
`A:\nsc-interop-build\claude\logs\lowering-tests.log`, `lowering-base1.log` y `lowering-base2.log`.
