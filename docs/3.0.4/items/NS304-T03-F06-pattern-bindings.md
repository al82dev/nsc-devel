# NS304-T03-F06 — Bindings `copy`, `move`, `ref` y `ref mut` en patterns

- **Estado**: EN IMPLEMENTACIÓN
- **Rama**: development
- **Alcance (SCOPE LOCK)**: `Language/SumTypeSemantics.h` (`PatternBindingMode`),
  `Parser::parsePatternPrimary` (modos), `Parser::lowerPattern`/`emitPatternBindings` (modo→HIR),
  y la prueba `TL-LANG-313`. NO tocar CMake, toolchain, Builder, foreach, rango T01-F04.

## Auditoría (línea base)

`PatternBindingMode` (SumTypeSemantics.h:44-47) tiene solo `Value`, `Reference`, `Move`.

Parseo actual de modo (Parser.cpp:10563-10587):
- `var x` → `Value`.
- `ref x` → `Reference` (solo lectura).
- `move x` → `Move`.

`copy` ya es keyword contextual (Parser.cpp:2100/13370) y `ref mut` se usa en `foreach`
(Parser.cpp:12287: "exclusive mutable alias"), pero NO en patterns.

## Gaps F06

1. **`copy`**: nuevo `PatternBindingMode::Copy`; parseo de `copy x` en `parsePatternPrimary`.
2. **`ref mut`**: nuevo `PatternBindingMode::ReferenceMutable`; parseo de `ref mut x`
   (dos contextuales) y lowering con alias mutable exclusivo.
3. Propagación del modo por el pipeline: `bindingModeName` (11400-11407), `collectBindings`,
   `emitPatternBindings` (11240/11251) y la validación de OR-compatibilidad
   (`orPatternBindingsCompatible` en SumTypeSemantics.cpp:55-72) que compara `mode`.

## Plan

1. Añadir `Copy` y `ReferenceMutable` al enum + `bindingModeName`.
2. Parsear `copy x` y `ref mut x` (distinguir `ref` de `ref mut` por `peek` de `mut`).
3. Bajar `ref mut` como referencia mutable (igual que el `foreach ref mut`).
4. Prueba `TL-LANG-313`: `copy`/`move`/`ref`/`ref mut` en un pattern de struct y de list.

## Prueba puntual

`TL-LANG-313`: bindings `copy`/`move`/`ref`/`ref mut` compilan y preservan el modo; `ref mut`
permite mutar el elemento; `ref` (readonly) rechaza escritura.
