# NS304-T13-F02 — Selección del allocator en construcción

- **Estado**: EN PROGRESO (gap confirmado; diseño = generalizar `new (expr) T(args)` con `IAllocator`)
- **Rama**: development

## Auditoría + diseño (2026-10-14)

- **Confirmado en O0**: `new (alloc) A(42)` se parsea como `PlacementConstructionExpression` y el
  binding exige `A*` exacto (`placement construction ... requires destination storage of exact type
  'type:...A*'`). El hueco: si `expr` conforma `IAllocator`, debe ser **selección de allocator**, no
  placement crudo.
- **Diseño (no es surface nueva)**: generalizar el shape ya decidido de Item 317 `new (expr) T(args)`
  por tipo — `expr : IAllocator` → `allocator.Allocate(size)` + construir in-place + devolver la
  referencia `T` (igual que placement de clase). Evalúa `expr` una sola vez; overload del constructor
  intacta. Sitio nuevo `ProjectConstructionSiteKind::Allocator` + transporte del allocator al lowering.

## Pendiente (vertical en curso)

- Binding (conformance a `IAllocator` + sitio `Allocator`), modelo de construcción, HIR/MIR y lowering
  `allocate`+construct en `lowerPlacementConstruction`.
