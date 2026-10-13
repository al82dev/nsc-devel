# NS304-T08-F02 — Extension properties con getter/setter/ref getter

- **Estado**: CERRADO (getter/setter) / residual (ref getter)
- **Rama**: development

## Resultado

- Parser: la property baja su getter a una `FunctionDeclaration` top-level con el nombre de la
  property + receptor (`f09b24da`, `bdc762f2`).
- Sema: `bindExtension` auto-invoca el getter receiver-only antes de registrar el method group.
- HIR: el member access auto-invocado reconoce el `NamespaceFunctionCallBinding` y lo baja como
  `StaticCall` con el receptor reenviado (`be5c3a25`).
- `TL-LANG-324` (`21.Doubled` = 42) PASA.
- **Setter (método)**: el accessor `set_` se baja a `FunctionDeclaration` y es invocable como
  método `21.set_Doubled(42)` (`TL-LANG-330`).
- **Setter (asignación)**: `21.Doubled = x` re-detecta el binding de extensión en
  `analyzeAssignment`, sintetiza el `NamespaceFunctionReferenceBinding` de `set_<name>` en el
  namespace del getter y lo baja por `analyzeNamespaceFunctionCall` (receptor reenviado + valor
  como argumento extra). HIR emite el `StaticCall` del setter en la rama de asignación.

## Residual

- **Getter por referencia** (`ref` en extension property): pendiente. Requiere que el accessor
  `get_` retorne por referencia y que la asignación reescrita mantenga la dirección del receptor
  mutable (hoy el receptor viaja por valor).
