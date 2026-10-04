# Baseline 2026-10-04: frontera de pánico del runtime

## Problema

La prueba histórica de Stage 45.8 exigía que el runtime no contuviera personalidad Win64 ni
funclets. Ese contrato quedó obsoleto cuando `__nativesharp_start` adquirió la frontera que impide
que un pánico de NativeSharp escape del proceso sin un código de salida determinista.

## Cambio

El verificador exige ahora:

- `personality ptr @__C_specific_handler` en el arranque;
- `panic.catchswitch` y `panic.catchpad`;
- el filtro exclusivo `__nativesharp_panic_eh_filter`;
- ausencia de un `cleanuppad` ajeno a esa frontera.

## Archivo

- `tests/unit/Stage45NativeExceptionRuntimeTests.cpp`

## Mejora

La prueba congela la protección real del punto de entrada y deja de rechazar una defensa que el
runtime necesita para terminar de forma estable ante pánicos no recuperables.

## Validación dirigida

- `nsc.stage45.native-exception-runtime`: 32/32 comprobaciones correctas.
