# NS304-T08-F01 — Bloques extension canónicos sobre tipos y constraints

- **Estado**: EN IMPLEMENTACIÓN (auditado)
- **Rama**: development

## Auditoría (hallazgo validado)

- El parser YA acepta `extension <Type> { ... }` y sintetiza el receptor `__nsc_extension_this`.
- **Causa raíz (hipótesis fuerte)**: `ProjectNameLookup::predeclareNode` solo registra un
  `FunctionDeclaration` como namespace function si `!namespaceSegments.empty()` (línea 613-614).
  Un `extension int { ... }` en el NAMESPACE GLOBAL tiene `namespaceSegments` vacío, así que su
  `Double()` NO se registra y `extensionCandidatesInNamespace` no lo encuentra. Se confirmaría
  registrando global-namespace functions (o probando el extension dentro de un `namespace`).

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
