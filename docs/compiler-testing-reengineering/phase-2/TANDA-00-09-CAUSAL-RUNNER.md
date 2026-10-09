# Tanda 00.09 — Coordinador causal de ejecución

## Resultado

`SuiteRunner` ejecuta iterativamente un conjunto seleccionado y cerrado por dependencias. Para cada caso:

1. espera a que sus productores tengan resultado;
2. materializa `BLOCKED` sin ejecutar el consumidor si un productor no pasó;
3. crea un workspace nuevo sólo cuando el caso realmente va a ejecutarse;
4. emite `CASE_STARTED` y `CASE_FINISHED`;
5. convierte una excepción del executor o una identidad incorrecta en `INFRASTRUCTURE_ERROR`.

La cancelación marca todos los pendientes como `CANCELLED`, conserva la causa y emite `RUN_CANCELLED`. Tanto una
corrida normal como una cancelada terminan con exactamente un resultado por seleccionado y `RUN_FINISHED`.

## Invariantes

- no hay recursión en el scheduler;
- productores se ejecutan antes que consumidores;
- un consumidor bloqueado no obtiene workspace ni invoca al executor;
- una excepción de infraestructura no se presenta como fallo del lenguaje;
- el stream progresivo describe cada transición confirmada.

## Evidencia enfocada

`tests/compiler-suite/controller/tests/test_runner.py` valida orden, bloqueo, excepción y cancelación completa con
executors sintéticos. No invoca NSC, NSLink ni contratos de producto.
