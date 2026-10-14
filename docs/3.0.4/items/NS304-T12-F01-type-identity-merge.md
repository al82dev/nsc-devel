# NS304-T12-F01 — Identidad y merge de tipos parciales

- **Estado**: AUDITADO (PartialTypeAggregator existe para tipos; identidad de members sin normativa)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar identidad y merge de tipos parciales*.
Plan T12-F01 (DeepSeekDocs): *auditar `PartialTypeAggregator`: misma identidad con orden de archivos
inverso produce igual modelo/metadata/diagnósticos; validar modifiers, base, traits, generics y
accessibility compatibles; diagnosticar contradicción en ambas declaraciones*.

Estado real:

- `PartialTypeAggregator` existe (agregación de tipos + orden estable), pero su cobertura es de TIPOS,
  no de members (methods/properties/events/constructors).
- La identidad canónica de member (owner, kind, nombre, aridad, passing modes, effects, constraints,
  static/instance, accessor kind) no está definida ni mergeada.

## Gap

Falta la identidad canónica de member parcial + el merge determinista (orden de archivos inverso →
mismo modelo) + el diagnóstico de contradicción de modifiers/base/traits/generics/accessibility.

## Nota

F01 es la base de T12: la clave de identidad semántica (no ruta/posición) que usan F02–F06. El
`PartialTypeAggregator` actual es el punto de partida, no la cobertura completa.
