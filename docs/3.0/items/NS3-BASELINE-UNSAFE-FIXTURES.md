# NS3 baseline: fixtures positivos bajo el contrato `unsafe`

## Problema

Después de activar el diagnóstico `NS3030` para Language 3, tres fixtures positivos seguían
ejecutando desreferencias, conversiones de puntero, `stackalloc` y toma de direcciones fuera de un
bloque `unsafe`. La validación fallaba antes de alcanzar los contratos que realmente pretendía
medir: operadores conscientes de null, pinning y resúmenes de escape.

## Cambio

Las operaciones de bajo nivel se delimitaron con bloques `unsafe` justificados. Los casos negativos
de escape conservan la operación explícitamente insegura para que el diagnóstico relevante siga
siendo la fuga de la dirección, no la ausencia de contexto `unsafe`.

## Archivos

- `tests/unit/NativeSharp3T01NullAwareOperatorTests.cpp`
- `tests/unit/NativeSharp3T06PinningTests.cpp`
- `tests/unit/NativeSharp3T06EscapeSummaryTests.cpp`

## Mejora

Los fixtures quedan alineados con Language 3 y vuelven a probar su feature específico sin debilitar
la auditoría de punteros del compilador.
