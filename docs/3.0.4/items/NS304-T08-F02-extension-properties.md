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

1. Parsear `public int Prop { get { ... } set { ... } }` dentro de un bloque `extension`, con el
   receptor sintético igual que los métodos.
2. Cablear el lookup de extension property (getter/setter/ref getter) al member access del Sema.

## Plan

1. En `parseExtensionDeclaration`, aceptar `PropertyDeclaration` además de `FunctionDeclaration`
   (reusar el parser de property de class member, inyectando el receptor).
2. En el lookup (Sema), resolver extension properties con el `bindExtension` (análogo a métodos).
3. Prueba `TL-LANG-324`.

## Prueba puntual

`TL-LANG-324`: `extension int { int Doubled { get { return this * 2; } } }` + `21.Doubled` — exit 42.
