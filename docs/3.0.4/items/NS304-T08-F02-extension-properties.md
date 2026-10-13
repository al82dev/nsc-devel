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

- El parser SÍ acepta `PropertyDeclaration` en el bloque extension (y se inyectó el receptor en el
  getter/setter), pero `21.Doubled` sigue fallando: el ACCESSOR (getter `get_Doubled`) es HIJO de la
  property y NO se registra como función de extensión top-level (`predeclareNode` no lo visita), así
  que el `extensionBinding`/`bindExtension` no lo encuentra. El lookup de extension property es un
  mecanismo aparte del de métodos.

## Plan

1. Registrar los accessors de la property (getter/setter) como funciones de extensión top-level
   (o hacer que el lookup de member access resuelva extension properties vía el getter).
2. Prueba `TL-LANG-324` (ya redactada; se re-registra al pasar).

## Prueba puntual

`TL-LANG-324`: `extension int { int Doubled { get { return this * 2; } } }` + `21.Doubled` — exit 42.
