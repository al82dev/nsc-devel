# SEM-04.05 — Evaluador iterativo de orígenes de escape

## Propósito

Eliminar la recursión restante de la consulta de orígenes de referencias sin cambiar el orden
ni la política conservadora de lifetimes.

## Arquitectura

`ProjectReferenceEscapeEvaluator` es un componente dedicado. Recorre expresiones mediante una
máquina iterativa y sólo conserva continuaciones explícitas para los dos casos que realmente
necesitan regresar al padre:

- unión ordenada de brazos de `match`;
- búsqueda ordenada entre argumentos elegibles de una llamada que retorna referencia.

Los shells, receivers, índices y dereferencias son transiciones de cola. Cada continuación vive
en un vector alojado en heap; la profundidad de la expresión ya no consume stack nativo.

## Contratos preservados

- mismo orden fuente para brazos y argumentos;
- mismo ranking conservador de orígenes en `match`;
- misma relación declarada de lifetimes;
- primer argumento elegible para indexadores y primer origen no externo para llamadas;
- identidad de parámetro, scope máximo y nombres `this`/`static` sin cambios.

## Archivos

- `ProjectReferenceEscapeEvaluator.h/.cpp`: evaluador y máquina iterativa;
- `ReferenceEscapeContinuationKind.h`: clases de continuación;
- `ReferenceEscapeEvaluationFrame.h`: estado explícito de una continuación;
- `ProjectMethodBodyModel.h/.cpp`: amistad acotada y delegación;
- `CMakeLists.txt`: registro explícito del nuevo componente en `nsc_frontend`;
- este documento.

## Validación dirigida

Construcción y ejecución exclusiva de Stage 50.7 en O0. No se ejecuta baseline, minibaseline,
CTest general ni O1/O2/O3.
