# Tanda 00.03 — Estados finales y bloqueo causal

## Resultado

La infraestructura define seis estados finales: `PASS`, `FAIL`, `BLOCKED`, `SKIPPED`, `CANCELLED` e
`INFRASTRUCTURE_ERROR`. Cada caso recibe un solo resultado final en un ledger append-only.

`ResultLedger` distingue tres situaciones del consumidor:

- ejecutable: todos los productores terminaron en `PASS`;
- esperando: todavía existen productores sin resultado;
- bloqueado: todos los productores terminaron y al menos uno no pasó.

Un resultado `BLOCKED` conserva las identidades exactas en `blocked_by`. No copia el error del productor, no usa
un artefacto anterior y no presenta al consumidor como un defecto independiente.

## Invariantes

- `PASS` no puede contener fallos ni bloqueadores;
- `FAIL` e `INFRASTRUCTURE_ERROR` requieren evidencia;
- `BLOCKED` requiere al menos un productor;
- sólo `BLOCKED` puede contener `blocked_by`;
- no se puede sobrescribir un resultado final;
- esperar no equivale a bloquear.

## Evidencia enfocada

`tests/compiler-suite/controller/tests/test_result.py` valida invariantes, append-only, espera parcial, liberación
por productores correctos y bloqueo causal por identidad.
