# NSC 3.0.5 — B02: extracción de scopes y flujo

Fecha: 2026-10-17
Rama de trabajo: `development`
Naturaleza: movimiento puro, sin cambio de lenguaje ni de algoritmo

## Objetivo

Separar del monolito `ProjectMethodBodyModel.cpp` la infraestructura que crea y consulta scopes,
captura/restaura el estado de flujo y combina salidas de ramas. La fachada pública y privada de
`ProjectMethodBodyModel` permanece intacta: sólo cambia la unidad de traducción que contiene las
definiciones.

## Frontera aplicada

El nuevo módulo `src/Frontend/Bodies/BodyScopeAndFlow.cpp` contiene:

- `createScope` y `scopeContains`;
- `lookupScopeName` y `lookupScopeNameInScope`;
- `seedParameters`;
- `captureOwnershipFlow` y `restoreOwnershipFlow`;
- `analyzeBranchState` y `commitBranchJoin`;
- `joinOwnershipFlow` y `joinOwnershipFlows`;
- los helpers privados de indexación de snapshots requeridos exclusivamente por esas operaciones.

`CMakeLists.txt` registra la nueva unidad en `nsc_frontend`.

## Frontera que no se invadió

B02 no cambia ni mueve:

- análisis de expresiones o dispatch, reservado para B03;
- resolución de llamadas, reservada para B04;
- assignments o members, reservados para B05;
- construcción o patrones, reservados para B06;
- moves, borrows, ownership o escape, reservados para B07;
- `analyzeBlock`, `analyzeStatement` o sus recorridos, reservados para B08.

En particular, los conjuntos y bindings de propiedad siguen siendo escritos por las mismas rutas.
B02 únicamente conserva el mecanismo existente para fotografiar, restaurar y unir ese estado.

## Contrato conservado

| Aspecto | Garantía |
|---|---|
| API | No cambió ninguna declaración ni firma de `ProjectMethodBodyModel`. |
| Ordinals | Scopes, parámetros, locals, calls y bindings conservan su orden previo. |
| Flujo | Se preservaron los mismos bits definite/maybe, reglas de reachability y joins. |
| Diagnósticos | `seedParameters` conserva IDs, severidad, rangos y textos. |
| Determinismo | El mismo modelo se obtiene con j1, j2 y j12 según el gate focal existente. |
| Lenguaje/ABI | No hay cambios de sintaxis, semántica, layout, ABI, NSMETA ni formatos. |

## Dependencias hechas explícitas

La nueva unidad incluye directamente el encabezado del modelo y los encabezados de diagnósticos
que usa. Los helpers de snapshot con linkage interno se alojan junto con el código que los consume.
Los helpers que todavía son necesarios para ownership/escape permanecen en el monolito para B07.
Los símbolos internos que quedaron sin consumidores después del movimiento se retiraron únicamente
de la unidad donde pasaron a estar muertos; no se cambió su lógica.

## Validación focal

1. Construcción:

   `cmake --build A:\nsc-devel\build-test-infrastructure --target nsc_project_method_body_model_tests -j 16`

   Resultado: PASS de construcción con `/W4 /WX`.

2. Ejecución:

   `A:\nsc-devel\build-test-infrastructure\tests\nsc_project_method_body_model_tests.exe`

   Resultado: `PASS: 76 checks`.

3. Integración del compilador:

   `cmake --build A:\nsc-devel\build-clang --target nsc -j 16`

   Resultado: `nsc.exe` construido; contrato static host-runtime: passed.

4. Higiene:

   `git diff --check`

   Resultado: sin errores.

No se ejecutaron baseline, deploy, instalación, batería completa ni consumidores externos. La
configuración de CMake informa una ruta histórica en C: para CTest, pero B02 no ejecutó ese binario;
los targets compilados y la prueba focal proceden de los builds en A:.

## Resultado estructural

`ProjectMethodBodyModel.cpp` deja de contener 533 líneas de scopes/flujo y la responsabilidad
queda localizada en una unidad coherente. Esta tanda no reclama por sí misma una mejora de tiempo:
su objetivo es reducir acoplamiento y permitir que B03-B08 continúen mediante familias verificables
sin mezclar movimientos con cambios algorítmicos.
