# 241 — Aserciones de versión del lenguaje 1.0.1 / Language-version assertions for 1.0.1

## Resultado / Outcome

**ES.** Los cierres Stage 48.6 y Stage 50.10 validan la línea de lenguaje vigente `1.0.1`, no la
identidad histórica `0.1`. La versión visible del compilador continúa permitiendo su sufijo único
`-dev+build...`.

**EN.** Stage 48.6 and Stage 50.10 validate the current `1.0.1` language line rather than the
historical `0.1` identity. The visible compiler version continues to allow its unique
`-dev+build...` suffix.

## Causa / Cause

La entrega 216 promovió las identidades del código y la documentación, pero dos pruebas de cierre
conservaron literales 0.1. Ambas describían correctamente que el lenguaje debe fijarse y el build
puede variar; únicamente quedó obsoleto el valor fijado.

Delivery 216 promoted code and documentation identities, but two closure tests retained 0.1
literals. Both correctly stated that the language line must be pinned while builds may vary; only
the pinned value became stale.

## Implementación / Implementation

- Stage 48.6 exige `kLanguageVersion == "1.0.1"` y conserva la derivación de
  `kCompilerVersion` desde la release.
- Stage 50.10 exige la misma línea y sólo requiere que la identidad de build exista.
- Los mensajes y comentarios ya no anuncian 0.1.

Stage 48.6 now requires `kLanguageVersion == "1.0.1"` while preserving compiler-version derivation
checks. Stage 50.10 requires the same line and only requires a nonempty build identity. Messages
and comments no longer advertise 0.1.

## Archivos / Files

- `tests/unit/Stage48_6CppCliHostClosureTests.cpp`
- `tests/unit/Stage50_10OwnershipExamplesDocumentationTests.cpp`
- `mkdown/241_aserciones_version_lenguaje_1_0_1.md`

## Verificación dirigida / Targeted verification

Se ejecutan únicamente Stage 48.6 y Stage 50.10 y después se reconstruye el compilador desde cero,
sin baseline.

Only Stage 48.6 and Stage 50.10 are executed, followed by a clean compiler rebuild without a
baseline.

## Commit de referencia / Reference commit

**Título ES:** Actualiza las aserciones de lenguaje a 1.0.1

**Title EN:** Update language assertions to 1.0.1

**Descripción ES:** Alinea los cierres Stage 48.6 y Stage 50.10 con la identidad vigente sin fijar
el sufijo variable de cada build.

**Description EN:** Align Stage 48.6 and Stage 50.10 closure checks with the current identity without
pinning each build's variable suffix.
