# 1.0.1 — Compatibilidad cruzada NSMETA / Cross-version NSMETA compatibility

## Resultado / Outcome

**ES.** DLL-P2 convierte el rango histórico soportado en una matriz ejecutable.
NSMETA 1.0–1.8 se escribe como bytes reales, se vuelve a leer sin alterar su
versión y se valida contra el contrato común del compilador actual. Nueve goldens
binarios quedan archivados y se leen en cada corrida sin regeneración.

**EN.** DLL-P2 turns the supported historical range into an executable matrix.
NSMETA 1.0–1.8 is written as real bytes, read back without changing its version,
and validated against the current compiler's common contract. Nine binary
goldens are archived and read on every run without regeneration.

## Archivos / Files

- `tests/unit/Stage48_2LibraryMetadataTests.cpp`: matriz 1.0–1.8, goldens y
  rechazo de minor futuro 1.9.
- `tests/fixtures/nsmeta/historical-1.{0..8}.nsmeta`: artefactos archivados.
- `CMakeLists.txt`: entrega al gate la raíz de fuentes para localizar los
  goldens.

- `tests/unit/Stage48_2LibraryMetadataTests.cpp`: 1.0–1.8 matrix, goldens, and
  future-minor 1.9 rejection.
- `tests/fixtures/nsmeta/historical-1.{0..8}.nsmeta`: archived artifacts.
- `CMakeLists.txt`: supplies the source root used to locate the goldens.

## Decisiones / Decisions

- Los campos posteriores a cada minor no se inventan al leer versiones antiguas.
- `NSC_P2_REFRESH_GOLDENS=1` es una operación explícita de mantenimiento; la
  prueba ordinaria nunca reescribe el árbol fuente.
- Major distinto y minor futuro se rechazan desde el header, antes de confiar en
  el payload.
- Fingerprint, target, Native ABI, Runtime ABI, mangling, object, delegate y
  exception ABI conservan diagnósticos independientes ya cubiertos por Stage 48.2.

- Fields introduced after a minor are not invented while reading old versions.
- `NSC_P2_REFRESH_GOLDENS=1` is an explicit maintenance operation; ordinary test
  runs never rewrite the source tree.
- A different major and a future minor are rejected from the header before the
  payload is trusted.
- Fingerprint, target, Native ABI, Runtime ABI, mangling, object, delegate, and
  exception ABI retain independently tested diagnostics.

## Evidencia / Evidence

```text
nsc_stage48_2_library_metadata_tests: 144/144 PASS
archived formats: NSMETA 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8
future rejection: NSMETA 1.9
build root: A:\NSC-DEVEL-BRANCHES\nsc-devel-dlls\build-clang-lldlink
```

No se instaló producto ni se usó `A:\nsc-devel`.
No product was installed and `A:\nsc-devel` was not used.

## Commit / Commit

```text
Certifica compatibilidad cruzada de NSMETA / Certify cross-version NSMETA compatibility
```
