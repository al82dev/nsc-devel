# NS304-T08-F07 — NSMETA y cero layout mutation

- **Estado**: AUDITADO (extensión no inyecta campos; NSMETA de extension pendiente)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar NSMETA de extension y cero layout mutation*.
Plan T08-F07 (DeepSeekDocs): *versionar properties/static/operators/conformance con receiver, passing
mode, constraints y witness; producer y consumer separados, sin fuentes; comparar layout antes/después
y demostrar igualdad; inspeccionar HIR/MIR/LLVM para cero field injection, boxing o sidecar oculto*.

Estado real:

- **Cero layout mutation**: la extensión es un namespace function (F01/F02/F03); no agrega campos ni
  desplaza el layout del tipo extendido (no hay field injection ni sidecar).
- **NSMETA**: sin descriptor de extension (properties/static/operators/witness no se publican).

## Gap

Falta publicar el descriptor de extension en NSMETA (receiver, passing mode, constraints, witness) y
demostrar el consumo intermodular sin fuentes. La igualdad de layout ya se cumple por construcción
(cero mutación); falta la prueba que lo demuestra.

### Verificación (2026-10-13)

- La extensión (F01–F04) es namespace function sin field injection: el layout del tipo extendido no
  muta (verificado en los expedientes F01–F04). El pendiente es el **descriptor NSMETA + consumo
  intermodular** (la cola de metadata compartida con T05-F09/T06-F08/T07-F08/T16).

## Nota

F07 es el cola de NSMETA de T08 (compartido con T05-F09/T06-F08/T07-F08). No es una surface nueva;
es el plumbing de metadata + la prueba de layout inmutable.
