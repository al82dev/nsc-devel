# NS304-T06-F08 — Congelar iterator ABI/NSMETA, LLVM, PDB y consumo por foreach

- **Estado**: PARCIAL (ABI + consumidor MoveNext/Current/foreach cerrados; NSMETA/PDB/intermodular pendientes)
- **Rama**: development

## Cierre parcial (ABI + consumidor) — verificado 2026-10-13

- **Layout**: `iterator:<T>` = `{ i32 state; void* frame; T item; void* moveNextFn }` (32 bytes para int).
- **Constructor**: fija `moveNextFn = Gen$moveNext`.
- **Sema**: `it.MoveNext()`/`it.GetEnumerator()` son method-group facade (`kIteratorFacadeTypeKey`),
  `it.Current` es core-op; `analyzeCall` registra el core binding (0 args).
- **HIR/LLVM**: `MoveNext` baja el receptor como ADDRESS (`&it`, muta el state) + call `moveNextFn`;
  `Current` lee el item (campo 2); `GetEnumerator` devuelve self.
- **Consumo**: `foreach (int x in Gen())` funciona (el enumerator pattern usa GetEnumerator/MoveNext/Current).
- Verificado: `Gen(){yield 1; yield 2;}` + `while(it.MoveNext()) sum+=it.Current` → exit 3; `foreach` → exit 3.
  Registrado `TL-LANG-381` (PASS). Commits `fa6d523f`, `9baa906a`, `ee7eb667`.

Queda (cola del ABI): **NSMETA** (publicar el iterator en metadata), **PDB** (state/locals en debug) y
**consumo intermodular** (iterator opaco cruzando módulos).
