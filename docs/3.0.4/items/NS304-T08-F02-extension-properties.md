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

- El parser SÍ acepta `PropertyDeclaration` y baja el getter a un método top-level con el nombre de
  la property (`f09b24da`), así que `21.Doubled()` (llamada) ya funciona como extension method.
- `21.Doubled` (property, sin `()`) sigue fallando: `bindExtension` lo devuelve como METHOD GROUP
  ("can only be used as a call target"). Falta AUTO-INVOCAR el getter (método zero-arg) cuando la
  extension property se usa como valor. La auto-invocación es un cambio Sema (analogo al
  `resolveSyntheticCall` de "list-pattern-length", pero para namespace functions).

## Plan

1. ✅ Parser: property → getter método top-level con receptor.
2. Auto-invocar el getter en `bindExtension` (un solo candidato zero-arg + contexto de valor).
3. Prueba `TL-LANG-324` (ya redactada; se re-registra al pasar).

## Prueba puntual

`TL-LANG-324`: `extension int { int Doubled { get { return this * 2; } } }` + `21.Doubled` — exit 42.
