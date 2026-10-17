# NSC 3.0.5 — B05: assignments y acceso a miembros

Fecha: 2026-10-17
Rama: `development`
Naturaleza: movimiento puro

## Objetivo

Separar del monolito las dos familias responsables de acceso a miembros y asignaciones, conservando
la fachada de `ProjectMethodBodyModel` y toda la semántica existente.

## Módulos

### `MemberAccessAnalyzer.cpp`

Contiene `analyzeMember` y sus helpers privados de:

- nombres de tipos candidatos;
- facades internas de struct, enum e iterator;
- clasificación del receptor y publicación del mismo `ExpressionResult`.

El bloque movido tiene 1,206 líneas e incluye también `stage38AccessorCandidates` y
`stage38OperatorCandidates`, que producen los candidatos consumidos por member/call binding. La
unidad resultante tiene 1,268 líneas.

### `AssignmentAnalyzer.cpp`

Contiene `analyzeAssignment` y helpers privados para:

- tipos genéricos de valor;
- detección de tipo de error;
- operadores shift usados por compound assignment.

El bloque movido tiene 2,201 líneas e incluye los lookups de candidatos/tipos y las consultas de
contexto, readonly receiver y backing field que assignment/member comparten. La unidad resultante
tiene 2,259 líneas.

Ambas unidades se registran explícitamente en `nsc_frontend`.

## Invariantes

- mismo lookup y shadowing;
- misma clasificación type/value/base/this/namespace receiver;
- mismas reglas de campos, propiedades, eventos y grupos de métodos;
- mismos places, addressability y writeback;
- mismas reglas simple/compound assignment;
- mismas conversiones, narrowing y shifts;
- mismos moves, borrows, escape y definite assignment;
- mismos bindings, ordinals, diagnostics, textos y rangos;
- sin cambios de sintaxis, ABI, layout, NSMETA o formatos.

No se cambiaron declaraciones del modelo, estructuras persistidas ni algoritmos. Los helpers que
siguen teniendo consumidores en el monolito se duplicaron con linkage interno únicamente cuando
era necesario mantener la frontera de unidad; los helpers sin consumidores restantes se movieron.

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

B05 elimina 3,407 líneas de lógica principal del monolito y deja assignments y members en unidades
independientes. La tanda no reclama una mejora porcentual: reduce acoplamiento y costo de cambio
antes de B06-B08.
