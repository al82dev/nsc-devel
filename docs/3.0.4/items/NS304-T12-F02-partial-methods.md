# NS304-T12-F02 — Métodos parciales

- **Estado**: AUDITADO (sin declaración/implementación separadas de members)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar métodos parciales*.
Plan T12-F02 (DeepSeekDocs): *separar declaration/implementation; emparejar firma completa; definir
cuándo el body es obligatorio u opcional y qué ocurre con la llamada a un método sin body; effects,
defaults, attributes, generic constraints y ownership coinciden; dos bodies o dos declarations
incompatibles son error determinista*.

Estado real:

- No hay surface de método parcial (declaración sin body + implementación separada). Un método con
  `partial` no se parsea/empareja.

## Gap

Falta la separación declaration/implementation de métodos parciales + el emparejamiento de firma
completa (effects/defaults/attributes/constraints/ownership) + el body obligatorio/opcional (llamada
a método sin body) + el error determinista de dos bodies/declarations.

## Nota

F02 depende de F01 (identidad canónica) y es la surface de `partial` en métodos. No es una surface
nueva de firma; es el emparejamiento + la regla del body opcional.
