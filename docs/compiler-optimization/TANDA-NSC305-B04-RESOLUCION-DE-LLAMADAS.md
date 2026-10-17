# NSC 3.0.5 — B04: resolución de llamadas

Fecha: 2026-10-17
Rama: `development`
Commit de movimiento: `c06066ec6`

## Objetivo

Extraer la familia completa de resolución de llamadas del monolito
`ProjectMethodBodyModel.cpp`, manteniendo idénticos selección de overloads, conversions,
ownership, diagnósticos y bindings.

## Alcance movido

`src/Frontend/Bodies/CallAnalyzer.cpp` contiene:

- `analyzeNamespaceFunctionGroup`;
- `analyzeNamespaceFunctionCall`;
- `analyzeCall`;
- `resolveSyntheticCall`;
- `NamedArgumentMap` y `mapArgumentsToParameters`;
- helpers privados requeridos por esas rutas para facades, snapshots e identificación de error.

La familia principal formaba un bloque contiguo de 4,448 líneas. Con los helpers que sólo consumían
las llamadas, salieron 4,602 líneas de `ProjectMethodBodyModel.cpp`. La nueva unidad tiene 4,702
líneas y se registra explícitamente en `nsc_frontend`.

## Contratos preservados

- candidatos y orden de evaluación;
- mapeo positional/named/default/variadic/parameter-pack;
- inference y conversion ranking;
- método, función de namespace o accessor seleccionado;
- detección de ambigüedad y razones de descarte;
- pass kinds, ownership, borrows y escape;
- call bindings, diagnostics, textos y rangos;
- sintaxis, ABI, layout, NSMETA y formatos públicos.

No se modificaron firmas en el header ni estructuras persistidas.

## Decisión algorítmica explícita

La auditoría posterior al movimiento encontró dos rutas con frontera incremental de candidatos no
dominados y una ruta sintética que compara todos los pares. No se sustituyó la última en B04.
`betterCandidate` expresa una relación parcial que incorpora conversiones estructurales y
dominancia de constraints; cambiar el orden de comparación sin una caracterización específica
podría cambiar qué llamada se considera ambigua.

Esto no se oculta como deuda resuelta: B04 es una tanda de modularización activada por costo y
riesgo estructural. Una optimización del ranking requiere perfil que demuestre costo material y un
gate de overloads/ambigüedades dedicado. El cuello medido actual no justifica introducir ese riesgo
en este movimiento.

## Validación

1. Build focal con `/W4 /WX`:

   `cmake --build A:\nsc-devel\build-test-infrastructure --target nsc_project_method_body_model_tests -j 16`

   Resultado: PASS.

2. Gate del modelo:

   `A:\nsc-devel\build-test-infrastructure\tests\nsc_project_method_body_model_tests.exe`

   Resultado: `PASS: 76 checks`.

3. Integración:

   `cmake --build A:\nsc-devel\build-clang --target nsc -j 16`

   Resultado: `nsc.exe` construido y contrato static host-runtime passed.

4. Higiene:

   `git diff --cached --check`

   Resultado: limpio antes del commit.

No se ejecutaron baseline, deploy, instalación, batería completa ni consumidores externos. La ruta
histórica de CTest en C: sólo fue informada por CMake; no se ejecutó ni modificó.

## Resultado

La resolución de calls queda aislada en una unidad coherente y testeable. B05 puede tratar
assignments/members sin cargar el cuerpo de overload resolution; B06 y B07 conservan sus fronteras.
Esta tanda no reclama una aceleración porcentual.
