# Baseline 2026-10-04: diagnósticos de ownership Stage 49/50

## Problema

Tres expectativas conservaban frases anteriores a la unificación de RAII, valores move-only y
parámetros owned. Las operaciones seguían rechazándose correctamente, pero el texto ahora distingue
un valor definitivamente movido de uno posiblemente movido en una unión de flujo.

## Cambios

- El destino de `move` reconoce el contrato “RAII or move-only”.
- El join condicional espera “possibly moved” en lugar de “moved-from”.
- El uso posterior de un parámetro owned espera la denominación canónica “parameter”.

## Archivos

- `tests/unit/Stage49RaiiMoveSemanticsTests.cpp`
- `tests/unit/Stage50_3OwnedParametersReturnsTests.cpp`

## Mejora

Las pruebas siguen exigiendo el rechazo y ahora comprueban la precisión de flujo actual sin fijar
terminología eliminada.
