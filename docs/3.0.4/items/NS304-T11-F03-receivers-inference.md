# NS304-T11-F03 — Receivers, inferencia y conversiones compuestas

- **Estado**: AUDITADO (extension receiver + inferencia nominal existen; span/lifetime compuesto pendiente)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar receivers e inferencia de spans*.
Plan T11-F03 (DeepSeekDocs): *integrar spans como extension receiver y argumentos inferidos; propagar
lifetime/element type por conversiones compuestas; no borrar restricciones al cerrar genéricos; probar
overloads competidores y nested views*.

Estado real:

- Extension receiver + inferencia de argumentos nominales existen (T08/T09).
- Spans como receiver/argumento inferido: sin integración con lifetime/element type; las
  conversiones compuestas (vista anidada) no propagan provenance.

## Gap

Falta integrar spans como extension receiver y argumentos inferidos, propagar lifetime/element type
por conversiones compuestas (nested views) y no borrar restricciones al cerrar genéricos.

## Nota

F03 depende de F01/F02 (ranking + no-copy). No es una surface nueva; es la integración de la vista
en receiver/inferencia/compuestas.
