# 277a — Alineación del diagnóstico de arrays inline / Inline-array diagnostic alignment

## Resumen / Summary

La prueba histórica de Stage 56 todavía exigía el texto `fixed array`, aunque el contrato actual
denomina `inline array` tanto a los arrays de extensión constante como a los arrays locales de
extensión evaluada en ejecución. La implementación ya emitía el diagnóstico correcto; el baseline
fallaba únicamente porque la expectativa conservaba la terminología anterior.

The historical Stage 56 test still required the text `fixed array`, although the current contract
uses `inline array` for both constant-extent arrays and local arrays whose extent is evaluated at
run time. The implementation already emitted the correct diagnostic; the baseline failed only
because its expectation retained the former terminology.

## Archivos modificados / Modified files

- `tests/unit/Stage56C2C4LanguageCompletionTests.cpp`
  - `testFixedArrayCannotCrossACallByValue()` ahora exige el diagnóstico canónico
    `an inline array cannot be passed by value`.
  - `testFixedArrayCannotCrossACallByValue()` now requires the canonical diagnostic
    `an inline array cannot be passed by value`.

## Decisión de diseño / Design decision

No se cambió el diagnóstico del compilador porque `inline array` describe la propiedad semántica
que importa en esta frontera: el almacenamiento forma parte de su contenedor o frame y no puede
cruzar una llamada por valor. Restaurar `fixed array` habría vuelto inconsistente la terminología
con los arrays multidimensionales y de extensión variable incorporados en la tanda 2.

The compiler diagnostic was not changed because `inline array` describes the semantic property
that matters at this boundary: its storage belongs to its container or frame and cannot cross a
call by value. Restoring `fixed array` would make the terminology inconsistent with the
multidimensional and variable-extent arrays introduced in batch 2.

## Evidencia dirigida / Focused evidence

- Ejecutable unitario `nsc_stage56_c2c4_language_completion_tests`.
- El caso rojo continúa rechazando `float[4]` como parámetro por valor; sólo se sincronizó la
  expectativa textual.

- Unit executable `nsc_stage56_c2c4_language_completion_tests`.
- The negative case continues to reject a by-value `float[4]` parameter; only the textual
  expectation was synchronized.

## Commit de referencia / Reference commit

**Título / Title:** `Alinea diagnóstico de arrays inline / Align inline-array diagnostic`

**Descripción / Description:**

`Actualiza la prueba histórica de Stage 56 para exigir la terminología canónica de arrays inline sin cambiar la regla semántica ni el diagnóstico del compilador. / Update the historical Stage 56 test to require the canonical inline-array terminology without changing the semantic rule or compiler diagnostic.`
