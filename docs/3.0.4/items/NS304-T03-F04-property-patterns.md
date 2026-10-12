# NS304-T03-F04 — Completar property patterns anidados, posicionales, relacionales y lógicos

- **Estado**: EN IMPLEMENTACIÓN (posicional pendiente)
- **Rama**: development

## Estado (validado)

- Relacionales (`< <= > >=`), lógicos (`not`/`!`, `and`/`&`), y anidados: LISTOS (`TL-LANG-311`).
- Posicional `T(a, b, ...)`: PENDIENTE.

## Auditoría (posicional)

- El parser trata `T(payload)` como `PatternKind::Variant` con UN payload, y RECHAZA el comma
  (`Parser.cpp:10790`: "exactly one payload pattern").
- El lowering de `Variant` (`Parser.cpp:10957-10973`) usa `Is<T>()` + `__nsc_variant_extract_<T>()`
  (tagged union), que NO sirve para posicional (deconstrucción por posición).
- La deconstrucción YA existe: `bindDeconstructionSource` (`ProjectMethodBodyModel.cpp:2781`)
  soporta tuplas y nominales con `Deconstruct`.

## Gap F04 (posicional)

1. Parser: permitir `T(a, b, c)` (múltiples payloads) como patrón posicional (nuevo
   `PatternKind::Positional` o `Variant` multi-payload).
2. Sema: bajar el posicional vía `bindDeconstructionSource` (mapear cada sub-patrón a la posición).

## Plan

1. `PatternKind::Positional` (Parser.h) + parseo de `T(a, b, c)`.
2. Lowering: para nominal con `Deconstruct`, emitir la deconstrucción posicional.
3. Prueba `TL-LANG-322`.

## Prueba puntual

`TL-LANG-322`: `match(point) { Point(x, y) => x + y, _ => 0 }` con `Point` que publica `Deconstruct`.
