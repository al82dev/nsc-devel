# NS304-T09-F09 — Fallo parcial y composición

- **Estado**: AUDITADO (drop de subobjetos existe; integración/partial-instance pendiente)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar fallo parcial y composición*.
Plan T09-F09 (DeepSeekDocs): *modelar subobjetos inicializados y drop order inverso; integrar
inheritance, records, `with`, events, required/init y debug; una instancia parcial nunca se publica
ni recibe destructor de objeto completo*.

Estado real:

- **Drop de subobjetos**: el MIR/destruction ya baja el drop de campos/subobjetos en orden inverso
  (`destructionSteps`, `emitOneStructuredCleanup`), y los constructores `throws` no existen aún (F10).
- **Partial instance**: sin modelado explícito de "objeto parcial" (una instancia cuya construcción
  falló a mitad); el drop de un objeto completo se aplica, no el parcial.

## Gap

Falta el modelo de objeto parcial (subobjetos ya inicializados al fallar) + su drop inverso exacto,
y la integración con inheritance/records/`with`/events/required/init/debug.

## Nota

F09 depende de F10 (constructores `throws E`, que aún no existen) y del drop de subobjetos ya
existente. Es un vertical de destrucción + composición, no una surface nueva.
