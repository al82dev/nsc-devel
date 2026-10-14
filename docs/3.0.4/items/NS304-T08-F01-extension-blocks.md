# NS304-T08-F01 — Bloques extension canónicos sobre tipos y constraints

- **Estado**: CERRADO (namespace + namespace global)
- **Rama**: development

## Resultado (validado)

- `extension int { int Double() {...} }` dentro de un `namespace` + `using` funciona: **`TL-LANG-323` PASS**
  (exit 42). El receptor sintético + `bindExtension` + lookup están cableados.
- **Namespace global**: `predeclareNode` ya registra las funciones con `namespaceSegments` vacío.
  Verificado: `extension int { Double() }` en el nivel superior → `21.Double()` = 42. **`TL-LANG-388`
  PASS**. Commit `0b0d8c49`.

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
