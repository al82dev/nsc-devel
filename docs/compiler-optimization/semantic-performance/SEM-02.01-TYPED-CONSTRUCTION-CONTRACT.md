# SEM-02.01 - Contrato tipado de sitios de construcción

## Propósito

Preparar una frontera explícita entre el análisis de cuerpos y la resolución de constructores.
La fase `ProjectMethodBodyModel` ya conoce el tipo, ownership y categoría de valor de cada
argumento; `ProjectConstructionModel` no debe redescubrir esos datos recorriendo el AST.

## Cambios

- `MethodExpressionTypeBinding` se extrajo a un encabezado independiente, sin alterar sus campos
  ni su semántica.
- `ProjectConstructionSiteKind` identifica construcción ordinaria o placement.
- `ProjectConstructionSite` transporta identidad del callable, secuencia estable por callable,
  tipo objetivo y bindings tipados de argumentos.
- El orden contractual será `(callableOrdinal, sequenceWithinCallable)`. Esto desacopla el orden
  observable del instante de descubrimiento y prepara el paralelismo determinista.

## Archivos nuevos

- `include/nativesharp/Frontend/MethodExpressionTypeBinding.h`: contrato reutilizable de una
  expresión tipada.
- `include/nativesharp/Frontend/ProjectConstructionSiteKind.h`: clase cerrada del sitio.
- `include/nativesharp/Frontend/ProjectConstructionSite.h`: transferencia tipada entre fases.

## Alcance

Este commit sólo introduce y conecta contratos. Todavía no cambia la producción ni el consumo de
sitios; por eso es mecánico, reversible y verificable antes de retirar el segundo recorrido.

## Validación dirigida

- Compilar `nsc_project_construction_tests` con warnings-as-errors.
- Ejecutar únicamente `nsc_project_construction_tests`.
- No ejecutar baseline, CTest general ni optimizaciones distintas de O0.
