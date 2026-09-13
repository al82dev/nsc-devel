# 286b — Cierre histórico NSMETA 1.13 / NSMETA 1.13 historical closure

## Resultado / Result

La matriz histórica conserva ahora bytes archivados de NSMETA 1.12 después de elevar el escritor a
1.13. El diagnóstico que prueba el rechazo de un minor futuro se deriva de la versión vigente en
vez de conservar el literal anterior. / The historical matrix now retains archived NSMETA 1.12
bytes after raising the writer to 1.13. The diagnostic that verifies rejection of a future minor is
derived from the current version instead of retaining the previous literal.

## Archivos e implementación / Files and implementation

- `tests/fixtures/nsmeta/historical-1.12.nsmeta`
  - Golden binario escrito por el codec 1.13 en modo de compatibilidad 1.12 y vuelto a leer por el
    lector actual.
- `tests/unit/Stage48_2LibraryMetadataTests.cpp`
  - Construye la expectativa de versión futura con `kMetadataFormatMinor + 1`.
- `mkdown/286b_2_0_nsmeta_1_13_baseline_closure.md`
  - Registra la causa, el alcance y la evidencia del arreglo de cierre.

## Decisión / Decision

Cada minor legible anterior necesita un archivo real bajo control de versiones; generar sólo el
modelo en memoria no detecta regresiones del codec binario. La expectativa negativa no debe repetir
el número actual porque cada avance de formato convertiría una prueba correcta en un falso fallo.
/ Every previously readable minor needs real version-controlled bytes; generating only an in-memory
model cannot detect binary codec regressions. The negative expectation must not duplicate the
current number because every format advance would turn a correct test into a false failure.

## Evidencia enfocada / Focused evidence

```text
ctest -R ^nsc.stage48_2.library-metadata$
1/1 passed
238 checks
```

## Mensaje de commit de referencia / Reference commit message

**Título / Title**

`Cierra compatibilidad histórica de NSMETA 1.13 / Close NSMETA 1.13 historical compatibility`

**Descripción / Description**

`Archiva el golden 1.12 y deriva el diagnóstico de minor futuro de la versión NSMETA vigente para mantener completa la matriz N-1/N. / Archive the 1.12 golden and derive the future-minor diagnostic from the current NSMETA version to keep the N-1/N matrix complete.`
