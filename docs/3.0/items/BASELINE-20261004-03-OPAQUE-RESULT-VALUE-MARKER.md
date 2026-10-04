# Baseline 2026-10-04: marcador de valor para `some Trait`

## Problema

El wrapper sintético de un resultado `some Trait` se devolvía mediante `move(...)`, pero su
declaración no llevaba el marcador `ns3-value`. El modelo de ownership lo clasificaba como un
struct copiable cuando la representación era una referencia de clase y rechazaba el movimiento
generado por el propio compilador.

## Cambio

- `ProjectOpaqueResultModel` conserva `opaque-result` como prefijo contractual y añade
  `ns3-value` al wrapper sintético.
- El factory generado continúa transfiriendo el wrapper mediante `move(...)`; ahora el modelo de
  miembros y el de cuerpos coinciden en que se trata de un valor move-only de edition 3.

## Archivos

- `src/Frontend/ProjectOpaqueResultModel.cpp`
- `docs/3.0/items/BASELINE-20261004-03-OPAQUE-RESULT-VALUE-MARKER.md`

## Mejora

Restaura los resultados opacos locales y entre módulos sin conceder copia implícita ni abrir una
excepción especial en el verificador de ownership. El prefijo de metadata `opaque-result|` se
mantiene intacto.

## Validación dirigida

- Target y prueba exacta: `nsc_ns3_t05_opaque_result_tests` / `nsc.ns3.t05.opaque-results`.
- No se ejecutó baseline ni CTest general.
