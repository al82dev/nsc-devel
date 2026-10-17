# NSC 3.0.5 — B07: ownership y escape

Fecha: 2026-10-17
Rama: `development`
Naturaleza: movimiento puro

## Objetivo

Aislar places, partial moves, borrows, provenance y propagación de escape del monolito de method
bodies sin modificar ninguna regla de memoria del lenguaje.

## `OwnershipAnalyzer.cpp`

La unidad de 812 líneas contiene 19 métodos:

- roots de views y variable arrays;
- `partialMovePlaceKey` y `storagePlaceKey`;
- containment/overlap de places;
- raíces y cadenas de referencias;
- detección de referencias mutables en conflicto;
- estado definite/maybe de partial moves;
- invalidación y consulta de borrows activos.

El helper `declaredLifetimeOutlives` viajó con esta unidad. Su recorrido del grafo de lifetimes ya
es iterativo mediante vector-worklist y conjunto de visitados.

## `EscapeAnalyzer.cpp`

La unidad de 297 líneas contiene:

- `referenceEscapeOriginOfExpression`;
- `parameterMayCarryEscape`;
- `mutableParameterEscapeSummary`;
- `noteParameterEscape`;
- `finalizeEscapeSummaries`;
- ranking privado de `EscapeClass`.

## Invariantes

- mismos place keys para locals, parameters, fields, indices y slices;
- misma política conservadora para provenance desconocida;
- mismas reglas de overlap, reborrow y mutable aliasing;
- mismos estados moved/maybe moved e invalidated/maybe invalid;
- misma propagación y punto fijo de escape;
- mismos ordinals, bindings, diagnostics y orden;
- sin cambios de sintaxis, ABI, layout, ownership público, NSMETA o formatos.

No se movieron statements, returns ni cleanup: permanecen para B08. Los accessors públicos de
estadísticas permanecen en la fachada del modelo.

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

Ownership y escape quedan físicamente separados en dos unidades coherentes. B08 puede concentrarse
en statements, exits y cleanup sin mezclar las reglas de aliasing/provenance.
