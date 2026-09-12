# 238 — Ventana de productor NSMETA N−1/N / NSMETA N-1/N producer window

## Resultado / Outcome

**ES.** El compilador 1.0.1 acepta los NSMETA históricos producidos por la release congelada 0.9.0
y los producidos por su propia línea 1.0.1. Releases más antiguas o distintas continúan rechazadas.

**EN.** Compiler 1.0.1 accepts historical NSMETA produced by the frozen 0.9.0 release and metadata
produced by its own 1.0.1 line. Older or unrelated releases remain rejected.

## Causa / Cause

La promoción de identidad del compilador de 0.9.0 a 1.0.1 mantuvo formato NSMETA y ABI compatibles,
pero `verifyCurrentCompiler` exigía igualdad textual con la release actual. Los goldens 1.0–1.11 se
decodificaban correctamente y después eran rechazados sólo por su productor 0.9.0.

Promoting the compiler identity from 0.9.0 to 1.0.1 preserved compatible NSMETA and ABI contracts,
but `verifyCurrentCompiler` required textual equality with the current release. The 1.0–1.11
goldens decoded correctly and were then rejected solely because their producer was 0.9.0.

## Decisión / Decision

La ventana se declara con dos identidades exactas: N=`1.0.1` y N−1=`0.9.0`. No se deduce restando
SemVer porque el salto desde major cero no representa una sucesión aritmética y aceptar cualquier
0.x ampliaría el contrato sin evidencia. Los sufijos de canal/build siguen sin afectar
compatibilidad.

The window declares two exact identities: N=`1.0.1` and N-1=`0.9.0`. It is not inferred by SemVer
subtraction because crossing major zero is not an arithmetic succession, and accepting arbitrary
0.x producers would broaden the contract without evidence. Channel/build suffixes remain
compatibility-neutral.

## Archivos / Files

- `src/Interop/LibraryMetadata.cpp`
- `docs/1.0.1/VERSION.md`
- `mkdown/238_ventana_productor_nsmeta_n_1.md`

## Verificación dirigida / Targeted verification

La matriz histórica Stage 48.2 debe aceptar sus goldens NSMETA 1.0–1.11 de 0.9.0 y conservar el
rechazo explícito de 0.2.0. Después se reconstruye el compilador desde cero, sin baseline.

The Stage 48.2 historical matrix must accept its 0.9.0 NSMETA 1.0-1.11 goldens while retaining the
explicit rejection of 0.2.0. A clean compiler rebuild follows, without a baseline.

## Commit de referencia / Reference commit

**Título ES:** Restaura la ventana N−1 de productores NSMETA

**Title EN:** Restore the N-1 NSMETA producer window

**Descripción ES:** Acepta metadatos 0.9.0 y 1.0.1 cuando sus contratos de formato y ABI también
son válidos, y conserva el rechazo de productores fuera de la ventana.

**Description EN:** Accept 0.9.0 and 1.0.1 metadata when their format and ABI contracts are also
valid, while rejecting producers outside the window.
