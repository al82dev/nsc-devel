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
- **Resultado**: `T(a,b)` ahora parsea como `PatternKind::Positional` y reporta error claro
  (`4a33d271`). El lowering productivo está pendiente.

## Hallazgo (lowering)

- Las TUPLAS son estructurales y NO admiten `.Item{i}`; se acceden por deconstrucción
  (`TupleDeconstructElement` en HIR).
- Intenté bajar el posicional como `DeconstructionDeclarationStatement` con fuente
  `source().Deconstruct()` (igual que el patrón `Tuple`), pero la fuente CALL no se analiza:
  "cannot initialize local '' of type '<missing-type>' from tuple". La deconstrucción hoy solo
  acepta una fuente DIRECTA (valor), no una llamada.
- **Para cerrar**: o bien el Sema resuelve la deconstrucción sobre una llamada (`Deconstruct()`),
  o bien el parser materializa el resultado del `Deconstruct()` en un temporal y deconstruye ESE
  temporal.

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
