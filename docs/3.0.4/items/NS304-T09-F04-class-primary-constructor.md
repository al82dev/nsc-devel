# NS304-T09-F04 — Primary constructor de clase

- **Estado**: PARCIAL (class primary ctor promote-to-fields) / residual (semántica params-in-scope C# 12)
- **Rama**: development

## Resultado

- **Parser** (`parseClassDeclaration`): `class Name(params)` se desuara a campos promovidos
  (`promoted-primary-field`) + un constructor `promoted-primary-constructor` que asigna los
  parámetros a los campos en orden textual (misma semántica que el struct/record primario de
  T02-F05: sin captura oculta ni ABI alternativo).
- Prueba `TL-LANG-344`: `Point p = new Point(3, 4); p.Sum()` (usa los campos promovidos) → 7 → **PASS**.

## Residual

- **Semántica C# 12**: en C# los parámetros de un primary constructor de clase están en scope en
  inicializadores/miembros sin promoverse automáticamente a campos; NativeSharp usa la semántica
  promote-to-fields (consistente con el struct primario ya cerrado).
