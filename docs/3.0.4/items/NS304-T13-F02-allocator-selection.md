# NS304-T13-F02 — Selección del allocator en construcción

- **Estado**: IMPLEMENTADO (`new (allocator) T(...)` → `IAllocator.Allocate` + placement; provenance = F07)
- **Rama**: development

## Resultado (2026-10-14)

- **Sema** (`PlacementConstructionExpression`): si el operando conforma `IAllocator` (via
  `interfaceModel_->classImplementsInterface`), el sitio es `ProjectConstructionSiteKind::Allocator`;
  se resuelve el `Allocate` no-estático del tipo operando y se transporta al binding
  (`allocatorTypeKey` + `allocatorAllocateMemberOrdinal`).
- **HIR** (`ProjectOopHirModel`): desugaring a `BlockExpression` → `allocator.Allocate(sizeof T)`
  (InstanceCall, receiver por valor o address según struct/class del allocator, argumento = size del
  layout) + `PlacementConstruction` sobre el `byte*` devuelto. Sin llamada LLVM directa a `Allocate`.
- **Prueba**: `TL-LANG-477` — `new (alloc) A(42)` sobre `BlockAllocator : IAllocator` → `a.X` exit 42
  (**PASS**); verificado también con allocator `struct` (exit 42).

## Residual (fuera del surface)

- **Provenance del allocator** (contrato de F07): el seguimiento de qué punteros salen de qué
  allocator es el vertical siguiente; F02 solo selecciona el allocator en la construcción.

## Auditoría + diseño (2026-10-14)

- **Confirmado en O0**: `new (alloc) A(42)` se parsea como `PlacementConstructionExpression`; el
  binding exige `A*`. El hueco: si `expr : IAllocator`, es selección de allocator (`Allocate(size)` +
  construir in-place), no placement crudo.
- **Bloqueo del primer intento (documentado)**: llamar `IAllocator::Allocate` por
  `functions_.find(mangledName)` desde el LLVM lowerer fallaba (método en otro code-unit, inlineado).
  El fix correcto fue **desugaring en HIR**, implementado arriba.

## Commits

- `e726bda9` — selección de allocator en construcción (TL-LANG-477).

## Nota de reemplazo — Congelamiento 3.0 (DEC-09, C-08)

La firma de `IAllocator` que describe este documento fue reemplazada en 3.0, y la nueva es la única que se acepta:

```
byte* Allocate(long bytes, int alignment);
byte* Resize(byte* ptr, long oldBytes, long newBytes, int alignment);
void Deallocate(byte* ptr, long bytes, int alignment);
```

`new (alloc) T(...)` llama `Allocate(sizeof(T), alignof(T))`, y el `Deallocate` del objeto recibe el mismo
tamaño y la misma alineación.

Una implementación con la firma anterior (`Allocate(int)`, `Resize(byte*, int, int)`, `Deallocate(byte*)`) recibe
en ese miembro un diagnóstico que nombra la firma nueva
(`docs/3.0/decisiones-congelamiento/32-P6-IALLOCATOR-FIRMA-UNICA.md`).
El texto de arriba se conserva como registro histórico de 3.0.4.
