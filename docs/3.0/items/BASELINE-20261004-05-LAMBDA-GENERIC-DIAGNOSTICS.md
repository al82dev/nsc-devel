# Baseline 2026-10-04: edición de lambdas y diagnóstico `where`

## Problema

El verificador histórico de lambdas encontró que una barrera de edición detenía los casos negativos antes de alcanzar el contrato específico que la prueba quería medir. Esa barrera ya no aplica. Además, el parser de `where` ahora admite restricciones de igualdad con `==`,
pero una expectativa conservaba el texto anterior que sólo mencionaba `:`.

## Cambios

- Los proyectos de Stage 43 no necesitan seleccionar una versión del lenguaje.
- Los dos casos de lambda sin tipos esperan la regla canónica de 3.0: cada parámetro de una lambda
  todavía no convertida necesita tipo explícito.
- El caso `ConstraintWithoutConstraintList` espera el diagnóstico actual que enumera `:` y `==`.

## Archivos

- `tests/unit/Stage43LambdasClosuresTests.cpp`
- `tests/unit/Stage44GenericsMonomorphizationTests.cpp`

## Mejora

Las pruebas negativas llegan a la regla que pretenden validar y dejan de confundir una barrera de
edición o un texto obsoleto con una regresión de lambdas o genéricos.
