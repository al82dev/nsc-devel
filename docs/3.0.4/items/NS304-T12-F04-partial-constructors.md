# NS304-T12-F04 — Constructores parciales

- **Estado**: AUDITADO (constructores normales existen; parcial sin surface)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar constructores parciales*.
Plan T12-F04 (DeepSeekDocs): *sólo un body puede contener initializer o llamada base/this; unificar
firma, effects y required/init obligations; aplicar cleanup parcial y `throws E` de T09 sin duplicar
inicialización*.

Estado real:

- Constructores normales + primary ctors (T09-F04) existen. Un constructor parcial (declaration +
  implementation separadas) no existe.

## Gap

Falta el constructor parcial: un solo body con initializer/base/this, firma/effects/required/init
unificados, y el cleanup parcial + `throws E` (T09-F09/F10) sin duplicar inicialización.

## Nota

F04 depende de F01/F02 (identidad/métodos) + T09 (required/init/throws). No es una surface nueva; es
el emparejamiento parcial del constructor.
