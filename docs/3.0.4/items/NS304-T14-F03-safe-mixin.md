# NS304-T14-F03 — Mixin seguro

- **Estado**: AUDITADO (expansión de macro existe; contextos + higiene sin cerrar)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar mixin seguro*.
Plan T14-F03 (DeepSeekDocs): *definir contextos donde una expansión puede introducir statement/
expression/member/type; capturas explícitas e higiénicas; nombres frescos no colisionan; rechazar
expansión de kind incorrecto antes de publicar AST*.

Estado real:

- `MacroExpansionEngine` expande macros; los contextos permitidos (statement/expression/member/type) +
  la higiene de capturas/nombres frescos + el rechazo de kind incorrecto no están cerrados.

## Gap

Falta definir los contextos de expansión + la higiene (capturas explícitas, nombres frescos sin
colisión) + el rechazo de kind incorrecto antes de publicar el AST.

## Nota

F03 depende de F02 (generación tipada) y es la higiene del mixin. No es una surface nueva; es el
cierre de la higiene de expansión.
