# NS304-T08-F02 — Extension properties con getter/setter/ref getter

- **Estado**: CERRADO (getter) / residual (setter/ref getter)
- **Rama**: development

## Resultado

- Parser: la property baja su getter a una `FunctionDeclaration` top-level con el nombre de la
  property + receptor (`f09b24da`, `bdc762f2`).
- Sema: `bindExtension` auto-invoca el getter receiver-only antes de registrar el method group.
- HIR: el member access auto-invocado reconoce el `NamespaceFunctionCallBinding` y lo baja como
  `StaticCall` con el receptor reenviado (`be5c3a25`).
- `TL-LANG-324` (`21.Doubled` = 42) PASA.

## Residual

- **Setter (método)**: el accessor `set_` ya se baja a `FunctionDeclaration` y es invocable como
  método `21.set_Doubled(42)` (`TL-LANG-330`). La sintaxis de asignación `21.Doubled = x` y el
  getter por referencia quedan pendientes (requieren la rama de asignación→setter).
