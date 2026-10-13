# NS304-T03-F04 — Completar property patterns anidados, posicionales, relacionales y lógicos

- **Estado**: EVIDENCIA TÉCNICA ADELANTADA COMPLETA; cierre secuencial pendiente.
- **Rama**: development

Este expediente conserva resultados focales PASS, pero no incrementa el conteo oficial: T03 está
en 3/8 y F04 es el siguiente ítem que debe cerrarse en orden.

## Estado (validado)

- Relacionales (`< <= > >=`), lógicos (`not`/`!`, `and`/`&`), y anidados: LISTOS (`TL-LANG-311`).
- La precedencia canónica es `!` unario, después `&` y finalmente `|`; los paréntesis vuelven a
  entrar en la gramática completa y preservan agrupación explícita.
- Posicional `T(a, b, ...)`: LISTO (`TP-PAR-357`, `TL-LANG-322`).

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
- La deconstrucción (`DeconstructionDeclarationStatement`) en el THENBLOCK del match arm no declara
  los targets: "cannot initialize local '' of type '<missing-type>' from <source>". El patrón `Tuple`
  tiene el mismo código pero su condición es `false` (nunca se analiza), así que el bug estaba
  latente; el posicional (condición `true`) lo expone.
- **Para cerrar**: cablear `DeconstructionDeclarationStatement` en el análisis del arm (declarar los
  targets y bajar la deconstrucción), o bajar el posicional por otra vía (temporal + accesos).

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

## Evidencia de precedencia lógica

- 2026-10-13: construcción incremental de `nsc`, 7/7 pasos.
- 2026-10-13: construcción incremental de `nsc_component_tests`, 4/4 pasos.
- 2026-10-13: `TL-LANG-311` PASS, 364,823 us. Distingue `!p & q`, `!p | q`,
  `!(p & q)`, la combinación `!p & q | r` y un guard explícito.
