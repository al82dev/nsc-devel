# NS304-T12-F01 — Identidad y merge de tipos parciales

- **Estado**: IMPLEMENTADO (identidad + merge determinista de tipos y members; canonical member identity = refinamiento)
- **Rama**: development

## Resultado (verificado O0 — 2026-10-14)

- **Tipo**: `PartialTypeAggregator` agrega por `TypeIdentity.key()` y ordena por
  (identity.key, sourceOrdinal, sourceOffset) → **independiente del orden de archivos**; valida
  kind, accessibility, base, abstract/sealed/static (cobertura de CF-COMPILATION-004).
- **Member**: el merge de members parciales ya funciona y es determinista: método parcial firma +
  cuerpo, property/accessor, constructor y event (TL-LANG-416..419 → **PASS**); la firma duplicada
  se rechaza (`duplicate method signature 'A()'`), y el orden inverso de archivos produce el mismo
  resultado (exit 3).
- Contradicción de modifiers en ambas declaraciones se diagnostica (abstract vs cuerpo → duplicate
  signature).

## Residual (fuera del surface)

- **Identidad canónica completa de member**: la clave semántica extendida (owner, kind, nombre,
  aridad, passing modes, effects, constraints, static/instance, accessor kind) no está definida
  como un átomo único reutilizable; hoy la identidad de sobrecarga (signature) ya cubre el merge y
  el diagnóstico de duplicados.

## Auditoría (línea base)

Contrato (tracking): *Implementar identidad y merge de tipos parciales*.
Plan T12-F01 (DeepSeekDocs): *misma identidad con orden de archivos inverso produce igual modelo/
metadata/diagnósticos; validar modifiers, base, traits, generics y accessibility compatibles*.
