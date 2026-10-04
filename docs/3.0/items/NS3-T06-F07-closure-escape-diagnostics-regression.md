# NS3-T06-F07 — Estabilidad de diagnósticos de escape en closures

## Problema

La semántica de captures explícitas seguía rechazando correctamente cuatro escapes peligrosos,
pero sustituyó los prefijos públicos de sus diagnósticos. El gate Stage 50.7 detectó la ruptura en
borrowed parameters, borrowed locals, RAII owners y owned parameters.

## Corrección

- Se restauraron los textos contractuales `cannot be captured by a lambda` para borrowed locals y
  parameters.
- Los owners RAII conservan su categoría específica en el diagnóstico.
- Los owned parameters mantienen el diagnóstico de escape y añaden la solución válida: una
  captura `move` explícita.
- No se relajó ninguna regla de lifetime, escape u ownership y no se modificaron expectativas.

## Archivos

- `src/Frontend/ProjectMethodBodyModel.cpp`: diagnósticos estables dentro de
  `collectLambdaCaptures`.
- `tests/unit/Stage50_7BorrowEscapeAnalysisTests.cpp`: contrato existente usado sin cambios.

## Evidencia puntual

`nsc_stage50_7_borrow_escape_analysis_tests.exe`: **38/38 checks**.

No se ejecutó baseline, minibaseline ni suite general.
