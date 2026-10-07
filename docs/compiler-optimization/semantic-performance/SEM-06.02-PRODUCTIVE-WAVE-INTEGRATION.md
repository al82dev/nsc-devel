# SEM-06.02 — Integración productiva de oleadas

**Estado:** implementado y verificado de forma dirigida

**Commit principal:** `c57b26ba0`

**Verificacion posterior:** `5313d0089`, `8d029723d`

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

El build dirigido de `nsc` y de los blancos afectados paso con warnings como errores. La matriz final obtuvo:

- scheduler: 12/12;
- modelado de cuerpos: 76/76;
- Driver O0 `j1/j2/j12`: 6/6;
- escape summaries: 8/8.

La cancelacion y el fallo se comprueban en el scheduler antes de publicar la oleada; el modelo coordinador solo
recibe resultados despues del exito completo. El mecanismo no ofrece aun cancelacion global entre todas las fases
del compilador ni un presupuesto global entre requests simultaneos de LanguageService.

No se ejecuta baseline, minibaseline, CTest general ni O1/O2/O3.
