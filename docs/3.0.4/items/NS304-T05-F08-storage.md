# NS304-T05-F08 — Storage en stack, caller, arena o allocator explícito; nunca heap obligatorio

- **Estado**: HECHO (surface `[FrameStorage(...)]` + ExplicitHeap malloc/free; default caller-stack + nunca-heap)
- **Rama**: development

## Resultado (surface `[FrameStorage(...)]` — 2026-10-14)

- **Parser**: `[FrameStorage(Stack|Arena|Heap)]` antes de `async` (namespace member y class member).
- **Wiring**: `ClassMemberSymbol.frameStorage` → HIR `ProjectOopHirFunction.frameStorage` → MIR
  `ProjectOopMirFunction.frameStorage` → LLVM `FunctionInfo.asyncFrameStorage`.
- **Lowering**: el await aloca el frame del producer según su storage: `CallerStack` → `alloca`
  (default); `ExplicitHeap`/`CallerArena` → `allocationFunction_` (malloc) + `free` exactly-once en
  el camino terminal (blocking: ready/error; async: ready/error del await). El invariante
  `asyncFrameHasImplicitHeap == false` se mantiene (el heap es opt-in explícito).
- **Prueba O0** (`TL-LANG-437`, exit 42): `[FrameStorage(Heap)]` con await bloqueante + anidado.
  Guards `TL-LANG-433/435/436/372/374/376/413` PASS.

## Nota

`CallerArena` comparte el camino de alocación explícita (malloc) con `ExplicitHeap`; la distinción
de pool de arena es una refinación posterior que no cambia la superficie ni el invariante never-heap.
