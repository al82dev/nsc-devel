# SEM-06.02 — Integración productiva de oleadas

## Propósito

Hacer que `ProjectCompilation::setCompilationJobs` alcance por primera vez el análisis de
method bodies sin permitir escrituras globales desde workers.

## Flujo

1. `ProjectCompilation` construye `ProjectMethodBodyBuildOptions` con `compilationJobs_`, la
   comprobación cooperativa de cancelación y el destino de error de ejecución.
2. El coordinador filtra serialmente los callables elegibles de la oleada.
3. `ProjectCallableWaveScheduler` produce un resultado aislado por slot.
4. Si un slot falla o se cancela, no se invoca el reductor y el modelo raíz no recibe estado
   parcial de esa oleada.
5. Si todos terminan, `ProjectCallableWaveReducer` publica por orden estable y produce la
   siguiente oleada deduplicada.

Los catálogos de nombres, miembros, interfaces, herencia y genéricos permanecen inmutables
durante la oleada. Cada worker crea y posee un shard completo; no existen pools anidados.

## Archivos

- `ProjectMethodBodyBuildOptions.h`;
- `ProjectMethodBodyModel.h/.cpp`;
- `ProjectCompilation.cpp`;
- este documento.

## Validación dirigida

Build de los blancos afectados y pruebas puntuales del scheduler, method bodies y escape en O0.
La equivalencia `j1/j2/j12` se cierra en el siguiente commit con un fixture productivo dedicado.
No se ejecuta baseline, minibaseline, CTest general ni O1/O2/O3.
