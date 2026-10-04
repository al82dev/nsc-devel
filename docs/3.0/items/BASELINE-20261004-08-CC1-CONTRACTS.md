# Baseline 2026-10-04: contratos negativos CC1

## Problema

Dos rechazos CC1 seguían fijando diagnósticos anteriores a T02-F06 y a los parámetros de lifetime.
El compilador rechazaba ambos programas, pero explicaba la regla 3.0 vigente.

## Cambios

- El inicializador owner-bearing exige exactamente un `..move(source)`.
- Un nombre usado en `where` debe ser parámetro genérico o parámetro de lifetime.
- Se actualizó el comentario histórico que todavía afirmaba que los drop flags no existían.

## Archivo

- `tests/unit/Cc1CorrectnessTests.cpp`

## Mejora

CC1 conserva pruebas negativas estrictas y sus mensajes describen las capacidades ya implementadas,
sin revivir limitaciones eliminadas.
