# NSC 3.0.5 — B06: construction y patterns

Fecha: 2026-10-17
Rama: `development`
Naturaleza: movimiento puro

## Objetivo

Separar aggregates/construction y deconstruction/match patterns del monolito, sin modificar orden
de análisis, diagnostics, bindings ni contratos del lenguaje.

## `ConstructionAnalyzer.cpp`

La unidad de 867 líneas contiene:

- `aggregateIsUnion`;
- `aggregateRequiresActiveMember`;
- `aggregateAllowsSynthesizedMember`;
- `aggregateSupportsSynthesizedMember`;
- `aggregateIsBorrowedView`;
- `aggregateElementTypeKeys`;
- `aggregateElementNames`;
- `bindAggregateInitializer`;
- `recordConstructionSite`.

Conserva la semántica de structs, tuples, fixed arrays, records derivados, unions, borrowed views,
generalized initializers, spreads y move-spreads.

## `PatternAnalyzer.cpp`

La unidad de 678 líneas contiene:

- `bindDeconstructionSource`;
- `analyzeMatchExpression`.

Conserva deconstruction, exhaustiveness, guards, list/rest patterns, variant tags, reference matches
y selección del tipo común de los arms.

Ambas unidades se registran explícitamente en `nsc_frontend`.

## Invariantes

- mismos ordinals y bindings de construction/aggregate/deconstruction/match;
- mismo orden fuente y misma selección de miembros;
- misma accesibilidad;
- mismas conversiones;
- mismas reglas de ownership, moves, borrows y partial moves;
- mismos diagnostics, severidad, texto y rango;
- sin cambios de sintaxis, ABI, layout, NSMETA ni formatos.

## Recursión residual, no ocultada

El movimiento hizo visibles tres recorridos históricos:

1. validación transitiva de members sintetizados de aggregates;
2. nested aggregate initializers;
3. detección de rest-view dentro de un match.

B06 no los transforma. Cambiar `bindAggregateInitializer` requiere una máquina de frames que
preserve exactamente el intercalado entre análisis de elementos, mutación de ownership, orden de
diagnósticos y publicación de bindings; un postorder previo cambiaría conducta observable. El
tracking activo define B06 como separación construction/patterns y B08 como la tanda iterativa.
Estos recorridos quedan como hallazgo explícito para la clasificación post-B08: sólo se activará una
correctiva adicional si el perfil o un caso profundo demuestra costo o riesgo real. No se declara
que la recursión global del compilador esté eliminada.

## Validación

1. Build focal con `/W4 /WX`:

   `cmake --build A:\nsc-devel\build-test-infrastructure --target nsc_project_method_body_model_tests -j 16`

   Resultado: PASS.

2. Gate del modelo:

   `A:\nsc-devel\build-test-infrastructure\tests\nsc_project_method_body_model_tests.exe`

   Resultado: `PASS: 76 checks`.

3. Integración:

   `cmake --build A:\nsc-devel\build-clang --target nsc -j 16`

   Resultado: `nsc.exe` construido; contrato static host-runtime passed.

No se ejecutaron baseline, deploy, instalación, batería completa ni consumidores externos. La ruta
histórica de CTest en C: sólo fue mostrada por CMake; no se ejecutó ni modificó.

## Resultado

Construction/aggregates y patterns quedan en unidades coherentes, con 11 métodos extraídos. B07
puede aislar ownership/escape y B08 statements sin volver a mezclar estas responsabilidades.
