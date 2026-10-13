# NS304-T08-F02 — Extension properties con getter/setter/ref getter

- **Estado**: EN IMPLEMENTACIÓN (auditado)
- **Rama**: development

## Auditoría

- El bloque `extension` solo acepta FUNCIONES (`parseExtensionDeclaration` usa
  `parseGlobalDeclaration()` y rechaza lo demás: "an extension block may contain only
  namespace-function declarations", Parser.cpp:2776-2782).
- Las PROPERTIES son class members (`parseClassMember`), no global declarations, así que el bloque
  extension no las parsea hoy.

## Gap F02 (validado con intento real)

- El parser baja el getter a una `FunctionDeclaration` top-level con el nombre de la property
  (`f09b24da` + `bdc762f2`), así `21.Doubled()` (llamada) ya funciona.
- `21.Doubled` (property) ya se AUTO-INVOCA en Sema (`bindExtension` auto-invoca el getter
  receiver-only antes de registrar el method group, `bdc762f2`).
- **Falta HIR**: el member access `21.Doubled` no baja como llamada de namespace ("Stage 34 cannot
  map identifier/member expression to Stage 27 binding"). Hay que reconocer el
  `NamespaceFunctionCallBinding` en el path de member access del HIR (extraer el bloque de
  ProjectOopHirModel.cpp:5703 a un helper y llamarlo desde el member access).

## Plan

1. ✅ Parser: property → getter `FunctionDeclaration` top-level con receptor.
2. ✅ Sema: auto-invocar el getter en `bindExtension`.
3. HIR: bajar el member access auto-invocado como llamada de namespace.
4. Prueba `TL-LANG-324` (ya redactada; se re-registra al pasar).

## Prueba puntual

`TL-LANG-324`: `extension int { int Doubled { get { return this * 2; } } }` + `21.Doubled` — exit 42.
