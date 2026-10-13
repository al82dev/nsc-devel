# NS304-T08-F02 — Extension properties con getter/setter/ref getter

- **Estado**: EN IMPLEMENTACIÓN (auditado)
- **Rama**: development

## Auditoría

- El bloque `extension` solo acepta FUNCIONES (`parseExtensionDeclaration` usa
  `parseGlobalDeclaration()` y rechaza lo demás: "an extension block may contain only
  namespace-function declarations", Parser.cpp:2776-2782).
- Las PROPERTIES son class members (`parseClassMember`), no global declarations, así que el bloque
  extension no las parsea hoy.

## Gap F02

1. Parsear `public int Prop { get { ... } set { ... } }` dentro de un bloque `extension` ✅ (parser
   acepta `PropertyDeclaration` en el bloque).
2. **Falta inyectar el receptor en el getter/setter**: el getter de la property no recibe
   `__nsc_extension_this`, así que `21.Doubled` falla ("member access ... receiver type builtin:int").
   El getter/setter son sub-nodos de la `PropertyDeclaration`; hay que inyectarles el receptor igual
   que a los métodos.
3. Cablear el lookup de extension property al member access del Sema.

## Plan

1. En `parseExtensionDeclaration`, al aceptar `PropertyDeclaration`, inyectar el receptor en el
   getter/setter (los sub-nodos).
2. En el lookup (Sema), resolver extension properties con el `bindExtension`.
3. Prueba `TL-LANG-324` (ya redactada; se re-registra al pasar).

## Prueba puntual

`TL-LANG-324`: `extension int { int Doubled { get { return this * 2; } } }` + `21.Doubled` — exit 42.
