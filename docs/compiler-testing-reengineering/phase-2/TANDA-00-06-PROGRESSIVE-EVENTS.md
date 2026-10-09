# Tanda 00.06 — Eventos progresivos y cancelación

## Resultado

`EventWriter` crea un `events.jsonl` nuevo y escribe una línea JSON completa por transición. Cada línea se vacía al
archivo y se sincroniza con el sistema antes de regresar, por lo que una interrupción conserva todos los eventos
confirmados hasta ese punto.

La secuencia inicia en cero y aumenta de forma monotónica. El writer rechaza un archivo existente para no mezclar
corridas ni sobrescribir evidencia. Un payload que no puede serializarse falla antes de escribir una línea parcial.

`CancellationToken` ofrece cancelación cooperativa thread-safe. La primera causa es autoritativa; solicitudes
posteriores no la reemplazan.

## Eventos iniciales

- `RUN_STARTED`;
- `CASE_STARTED`;
- `CASE_FINISHED`;
- `RUN_CANCELLED`;
- `RUN_FINISHED`;
- `INFRASTRUCTURE_ERROR`.

## Evidencia enfocada

`tests/compiler-suite/controller/tests/test_events.py` valida persistencia inmediata, secuencia, protección de
evidencia previa, ausencia de líneas parciales, cierre y causa estable de cancelación.
