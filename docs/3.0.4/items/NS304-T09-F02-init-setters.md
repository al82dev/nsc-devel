# NS304-T09-F02 — Setters `init` y congelamiento posterior a construcción

- **Estado**: PARCIAL (parser reconoce `init`) / residual (restricción de construcción + congelamiento)
- **Rama**: development

## Resultado

- **Parser** (`src/Parser/Parser.cpp`): `init` se reconoce como keyword contextual en las listas de
  accessors de property e indexer (`{ get; init; }`), igual que `set`. El accessor resultante se
  baja como `set_<name>` (misma vía que `set`), de modo que `p.Value = 42` compila y despacha.
- Prueba: `TL-LANG-334` (`int Value { get; init; }` + `p.Value = 42` → 42).

## Residual

- **Restricción de contexto**: `init` debe ser asignable SOLO durante la construcción (object
  initializer o constructor), no en cualquier contexto como hoy.
- **Congelamiento posterior a construcción**: marcar el backing field como de solo lectura una vez
  terminada la construcción.

Ambos requieren rastrear el contexto de construcción en Sema (no existe aún) y un marcador que
distinga `init` de `set` sin pisar el `auxiliaryText` que reconoce getter/setter.
