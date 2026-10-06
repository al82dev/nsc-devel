# SEM-06.01 — Planificador acotado de oleadas

## Propósito

Definir el mecanismo concurrente antes de conectarlo al modelo productivo. El planificador debe
reutilizar `CompilationWorkerPool`, conservar slots estables y no publicar resultados parciales.

## Contrato

`ProjectCallableWaveScheduler` recibe ordinales de callable, ordinal de oleada, número de jobs,
callback de análisis y comprobación cooperativa de cancelación.

- `-j1` usa un fast path síncrono con el mismo callback y el mismo tipo de resultado;
- `-jN` escribe cada resultado en un slot exclusivo preasignado;
- el orden del vector publicado es el orden de entrada, no el de finalización;
- una excepción cancela las tareas pendientes mediante el pool compartido;
- cancelación o fallo dejan el vector público vacío;
- el scheduler no fusiona estado semántico y no crea pools anidados.

## Aseguramiento dirigido

La prueba sintética cubre `j1`, `j2`, slots estables, cancelación y fallo. Comprueba propiedades
estructurales; no depende de texto exacto, hashes, conteos congelados ni tiempos.

No se ejecuta baseline, minibaseline, CTest general ni optimizaciones O1/O2/O3.
