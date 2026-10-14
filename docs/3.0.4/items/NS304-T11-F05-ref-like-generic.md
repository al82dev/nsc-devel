# NS304-T11-F05 — Ref-like como argumento genérico

- **Estado**: AUDITADO (constraints genéricos existen; `ref-like` stack-only sin surface)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar ref-like como argumento genérico*.
Plan T11-F05 (DeepSeekDocs): *agregar constraint explícito que autorice stack-only; negar boxing,
field en heap/objeto escapable, static storage y metadata inválida; specialization cache distingue el
contrato ref-like*.

Estado real:

- `ProjectGenericConstraintModel` + specialization ya existen para constraints nominales; no hay un
  constraint `ref-like`/`stack-only`.
- No hay negación de boxing/field escapable/static storage para un argumento genérico ref-like.

## Gap

Falta el constraint `ref-like` (stack-only): autoriza el uso genérico de una vista, y niega boxing,
field en objeto escapable, static storage y metadata inválida. La specialization cache debe distinguir
el contrato ref-like.

## Decisión de surface (material, para Alberto)

- **(A) constraint `ref-like`** (recomendada): `where T : ref-like` autoriza stack-only.
- **(B) `struct T` + `stack-only`** (dos constraints).

Recomendación: **(A)**.

## Nota

F05 depende de la surface del constraint ref-like (A/B) y de F01/F02 (span). No es una surface de
vista; es el constraint del sistema de tipos.
