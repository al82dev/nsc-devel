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

- **Setter**: el accessor `set_` ya se baja a `FunctionDeclaration` con receptor + value (`f6916a13`).
  Falta la RESOLUCION de asignacion `21.Doubled = x` → setter (el camino de `analyzeAssignment`
  debe derivar `set_Doubled` y ligar la llamada). El getter por referencia tambien queda pendiente.
