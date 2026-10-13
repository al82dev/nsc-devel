# NS304-T08-F01 — Bloques extension canónicos sobre tipos y constraints

- **Estado**: EN IMPLEMENTACIÓN (auditado)
- **Rama**: development

## Auditoría (hallazgo validado)

- El parser YA acepta `extension <Type> { ... }` (`parseExtensionDeclaration`, Parser.cpp:2716)
  y sintetiza el receptor `__nsc_extension_this`.
- **Infraestructura parcialmente cableada**: `ProjectMethodBodyModel::bindExtension` (13850) YA
  resuelve extensiones (receiver conversion + pass), y `ProjectNamespaceFunctionModel` crea
  `extensionBindingBySyntax_` para `MemberExpression` con candidatos `isExtension` (955-983).
- **Falta el eslabón**: `21.Double()` falla ("member access ... receiver type builtin:int"), es
  decir que el `extensionBinding` no se crea/encuentra para este caso — probablemente la
  `candidatesInNamespace` no registra el extension de `int` o el walk no lo visita con el nombre
  correcto. Es un gap de registro/lookup, no de sintaxis.

## Gap F01

1. Cablear el lookup de extension members: cuando un member access falla sobre un tipo, buscar en
   los bloques `extension` aplicables (receiver por value/ref/in) e inyectar el receptor sintético.
2. Constraints (extension sobre tipos genéricos con constraints).

## Plan

1. En `ProjectMemberModel`/`ProjectMethodBodyModel` (member lookup), añadir el fallback a
   extension members y la conversión del receiver.
2. Prueba `TL-LANG-323` (ya redactada; se re-registra al pasar).

## Prueba puntual

`TL-LANG-323`: `extension int { int Double() { return this * 2; } } 21.Double()` — exit 42.
