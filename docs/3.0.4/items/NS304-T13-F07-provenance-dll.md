# NS304-T13-F07 — Provenance intermodular/DLL

- **Estado**: FASE 1 — contrato e invariantes asentados (implementación por fases)
- **Rama**: development

## Hueco concreto (demostrado)

`owned A a = new (alloc) A(42);` (clase, `alloc : IAllocator`) → al droppear `a`, el destructor corre
(`Drops==1`) pero `alloc.Deallocate` no se llama (`Deallocs==0`). La provenance del allocator no viaja con
el objeto. En LLVM, `emitRaiiDestroy` usa el descriptor campo 7 (destruye vida, deja storage) porque
`storageOwnership == Borrowed`, pero nadie llama `alloc.Deallocate(storage)`.

## Decisión (recibida)

Header persistente con **contexto del allocator** + **thunk de deallocation con ABI estable** (no un
simple puntero a función, no un ordinal de miembro dependiente del productor). Cumple escape/retorno/
reasignación/almacenamiento/cross-module.

## FASE 1 — Contrato e invariantes

1. **Header por objeto clase** (prependido, el objeto apunta al inicio del bloque = descriptor):
   - offset 0: `descriptor` (vtable) — existente.
   - offset 8: `allocatorContext` (`void*`) — instancia concreta del allocator, o `null` para el predeterminado.
   - offset 16: `deallocateThunk` (`void(void* context, void* ptr)`) — ABI estable.
   - payload (campos de usuario) empieza en offset 24.

2. **Tamaño total** = `headerSize(8) + 16 + payload`. `Allocate` recibe el tamaño completo y `Deallocate`
   recibe **el mismo puntero** (inicio del bloque). El puntero visible de objeto y el puntero devuelto al
   allocator son **idénticos** (no hay offset split).

3. **Thunk ABI**: `void deallocate_thunk(void* context, void* ptr)`.
   - Predeterminado: `context == null`, thunk = free del runtime (ignora context).
   - Personalizado: el compilador genera un thunk por tipo de allocator que hace
     `((T*)context)->Deallocate((byte*)ptr)`.

4. **Estado predeterminado**: `allocatorContext = null`, `deallocateThunk = runtime_free_thunk`.
   Todo `new T(...)` sin allocator lo inicializa así (comportamiento previo conservado).

5. **Vida del allocator**: el contexto almacenado es una referencia cruda sin ownership. Invariante: un
   objeto creado con `new (alloc) T(...)` no puede sobrevivir a su allocator. En esta pasada la regla es
   **contractual + entidad estable**: las pruebas focales usan un allocator global/heap-owned que vive
   más que el objeto; el `scope ArenaAllocator` local queda sujeto a una comprobación de escape por
   provenance (pendiente, documentada, no silenciada).

6. **Doble liberación**: el cleanup guard (slot del owner a `null` antes del drop) sigue siendo la
   autoridad de "exactamente una vez"; el thunk solo se invoca desde ese camino.

7. **Fallo de constructor**: si el ctor lanza tras `Allocate`, la liberación usa la MISMA provenance
   (thunk + contexto) ya inicializada en el header antes de invocar el ctor.

## Pendiente (implementación, por fases)

- FASE 2: `ClassObjectLayout` + autoridad única de offsets/tamaños.
- FASE 3: construcción (`Allocate` tamaño completo, inicializar provenance antes del ctor, rollback).
- FASE 4: destrucción (drop-glue una vez → leer provenance → invocar thunk → impedir doble free).
- FASE 5: cross-module (descriptor/NSMETA solo si hace falta capacidad, sin gate de versión).
- FASE 6: vida del allocator (escape por provenance).

## Commits

- `843268c1` preparación semántica (`allocatorDeallocateMemberOrdinal`) — no cierra.
- `f83f8d5c` evidencia diagnóstica — no cierra.
