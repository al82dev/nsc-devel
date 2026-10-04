# NS3-T02-F06 — Spreads de agregados / Aggregate spreads

**Estado / Status:** TERMINADA / COMPLETE

## Cierre completo / Full closure

El camino productivo de agregados triviales quedó endurecido y comprobado de parser a PE x64. Una
fuente exacta se evalúa una vez; los campos explícitos tienen precedencia; HIR/MIR conservan los
offsets y LLVM copia sólo los campos faltantes en el destino final. La operación no asigna heap.

The production path for trivial aggregates is hardened and tested from parser through x64 PE. One
exact source is evaluated once, explicit fields win, HIR/MIR preserve offsets, and LLVM copies only
missing fields into final storage. The operation performs no heap allocation.

La mitad move-only usa `..move(source)` únicamente en un initializer de local. El source debe ser
un local move-only nombrado o un parámetro por valor; sólo se transfieren los fields no escritos,
cada ranura transferida se vacía inmediatamente y los siblings no movidos continúan vivos. El
análisis de ownership registra las rutas parciales y rechaza su lectura posterior.

The move-only half uses `..move(source)` only in a local initializer. The source must be a named
move-only local or by-value parameter; only unwritten fields are transferred, every transferred
slot is cleared immediately, and untouched siblings remain live. Ownership analysis records the
partial paths and rejects subsequent reads from them.

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

## Ownership, cleanup y unwind

T06-F01/F02/F08 ya proporcionan estado moved por field, drop de values y glue genérico. F06 los
consume sin crear una segunda regla: el destino se zero-inicializa y su cleanup se activa antes de
evaluar entradas que pueden lanzar. En unwind se destruyen los fields ya instalados; el source
destruye sólo sus siblings no transferidos. Un destructor personalizado sigue rechazando
move-spread porque puede observar el valor completo parcialmente movido.

T06-F01/F02/F08 now provide per-field moved state, value drops, and generic drop glue. F06 consumes
those contracts without inventing a second ownership model: the destination is zero-initialized
and its cleanup becomes active before potentially throwing entries are evaluated. Unwind destroys
installed fields, while the source destroys only untouched siblings. A custom destructor still
rejects move-spread because it may observe the complete partially moved value.

## Archivos / Files

- Nuevo: `tests/unit/NativeSharp3T02AggregateSpreadTests.cpp`.
- Nuevo: `docs/3.0/language/aggregate-spreads.md`.
- Modificado: `src/Frontend/ProjectMethodBodyModel.cpp`.
- Modificado: `include/nativesharp/Frontend/ProjectMethodBodyModel.h` para transportar la identidad
  del root de un move-spread.
- Modificado: `src/Frontend/ProjectOopHirModel.cpp` y dumpers HIR/MIR para conservar y publicar
  `mode=copy|move`.
- Modificado: `src/LLVM/ProjectOopLlvmLowerer.cpp` para copiar, vaciar la ranura fuente y activar
  cleanup durante inicialización parcial/unwind.
- Modificado: `CMakeLists.txt` y documentos de T02.

## Evidencia final / Final evidence

- `nsc_ns3_t02_aggregate_spread_tests`: `PASS: 19 checks` en O0 con NSLink.
- Incluye copy-spread, move-spread, sibling vivo, tres drops exactos, unwind parcial, use-after-move,
  alias owned, destructor personalizado, layout HIR/MIR y restricciones previas.
- Target exacto `nsc`: construido después de la prueba dirigida.

No se ejecutó baseline, certificación, O1/O2/O3, instalación, deploy ni escritura en `C:`.
