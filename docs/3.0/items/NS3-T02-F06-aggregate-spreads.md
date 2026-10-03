# NS3-T02-F06 — Spreads de agregados / Aggregate spreads

**Estado / Status:** IMPLEMENTANDO / IMPLEMENTING

## Cierre de la mitad Copy / Copy-half closure

El camino productivo de agregados triviales quedó endurecido y comprobado de parser a PE x64. Una
fuente exacta se evalúa una vez; los campos explícitos tienen precedencia; HIR/MIR conservan los
offsets y LLVM copia sólo los campos faltantes en el destino final. La operación no asigna heap.

The production path for trivial aggregates is hardened and tested from parser through x64 PE. One
exact source is evaluated once, explicit fields win, HIR/MIR preserve offsets, and LLVM copies only
missing fields into final storage. The operation performs no heap allocation.

## Defecto corregido / Fixed defect

Los generalized initializers enumeraban todos los campos físicos pero nunca comprobaban su
accesibilidad. Código externo podía escribir un campo `private` por nombre o por posición. F06
ahora conserva los member ordinals junto a los slots y aplica
`ProjectMemberAccessibility::isAccessible` a toda entrada explícita. Un spread opaco continúa
siendo válido porque copia un valor y no nombra la representación privada.

Generalized initializers enumerated every physical field but never checked accessibility. External
code could write a `private` field by name or position. F06 now keeps member ordinals beside slots
and applies the canonical accessibility rule to every explicit entry. An opaque spread remains
valid because it copies a value without naming private representation.

## Evidencia dirigida / Directed evidence

- `nsc_ns3_t02_aggregate_spread_tests`: `PASS: 10 checks`;
- `nsc_ns3_t01_generalized_initializer_tests`: `PASS: 9 checks`;
- `nsc_ns3_t01_with_expression_tests`: `PASS: 7 checks`;
- O0 y NSLink local exclusivamente;
- precedencia, orden observable `23`, fuente temporal, nesting, privados, duplicados, fuente
  única, identidad nominal, HIR, MIR y `noheap`;
- owner-bearing spread rechazado antes de lowering.

## Dependencia que impide TERMINADA / Completion dependency

La mitad move-only no puede activarse honestamente mientras los value locals con campos `owned`
no tengan drop flags y drop glue productivos. La implementarán T06-F01/F02/F08; entonces sus
pruebas de use-after-partial-move y conteo exacto de destructores cerrarán también este ítem. No se
declara soporte parcial inseguro.

The move-only half cannot be enabled honestly until value locals with `owned` fields have
production drop flags and drop glue. T06-F01/F02/F08 will implement them and will close this item
with use-after-partial-move and exact destruction-count tests. Unsafe partial support is not
claimed.

## Archivos / Files

- Nuevo: `tests/unit/NativeSharp3T02AggregateSpreadTests.cpp`.
- Nuevo: `docs/3.0/language/aggregate-spreads.md`.
- Modificado: `src/Frontend/ProjectMethodBodyModel.cpp`.
- Modificado: `CMakeLists.txt` y documentos de T02.

No se ejecutó baseline, certificación, O1/O2/O3, instalación, deploy ni escritura en `C:`.
