# Baseline 2026-10-04: edición de lambdas y diagnóstico `where`

## Problema

El verificador histórico de lambdas construía proyectos sin edición explícita. El frontend detenía
los casos negativos en la barrera de Language 2 antes de alcanzar el contrato específico que la
prueba quería medir. Además, el parser de `where` ahora admite restricciones de igualdad con `==`,
pero una expectativa conservaba el texto anterior que sólo mencionaba `:`.

## Cambios

- Los proyectos de Stage 43 se declaran con `<Language>3</Language>`.
- Los dos casos de lambda sin tipos esperan la regla canónica de 3.0: cada parámetro de una lambda
  todavía no convertida necesita tipo explícito.
- El caso `ConstraintWithoutConstraintList` espera el diagnóstico actual que enumera `:` y `==`.

## Archivos

- `tests/unit/Stage43LambdasClosuresTests.cpp`
- `tests/unit/Stage44GenericsMonomorphizationTests.cpp`

## Mejora

Las pruebas negativas llegan a la regla que pretenden validar y dejan de confundir una barrera de
edición o un texto obsoleto con una regresión de lambdas o genéricos.
