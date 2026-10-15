# NS304-T13-F07 — Provenance intermodular/DLL

- **Estado**: EN CURSO (hueco concreto demostrado; plumbing estructural en WIP)
- **Rama**: development

## Hueco concreto (2026-10-14)

`owned A a = new (alloc) A(42);` con `A` clase y `alloc : IAllocator` → al droppear `a`, el destructor
`~A()` corre (`Drops == 1`) pero `alloc.Deallocate` **no** se llama (`Deallocs == 0`). La provenance del
allocator se pierde: el sitio de construcción registra `Allocate` pero no `Deallocate`, y el objeto no
recuerda con qué allocator fue asignado para liberarlo con el correcto.

## Progreso (WIP `843268c1`)

- `ProjectConstructionSite::allocatorDeallocateMemberOrdinal` + `ObjectConstructionBinding::allocatorDeallocateMemberOrdinal`.
- Sema (`analyzeExpression` placement) resuelve y registra el miembro `Deallocate` junto a `Allocate`.
- Construction model transporta el ordinal. Compila.

## Frontera exacta / pendiente

- **Último valor correcto observado**: `allocatorDeallocateMemberOrdinal` resuelto (ordinal del `Deallocate`).
- **Primer valor ausente**: el consumo en el lowering de destrucción — el objeto clase no lleva el
  allocator (per-object) ni llama `alloc.Deallocate(storage)` al liberar.
- **Qué falta**: enganchar la deallocación a `alloc.Deallocate(storage)` en el camino de destrucción del
  objeto (`lowerObjectDestruction` / destruction entry), sin doble-free con el GC. Es un vertical de
  codegen/runtime (provenance per-object), no de surface ni de versionado NSMETA.
