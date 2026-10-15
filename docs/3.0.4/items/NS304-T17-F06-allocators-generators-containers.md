# NS304-T17-F06 — Allocators + generators + containers + noheap

- **Estado**: COMPLETO — la combinación allocator + generator + container + noheap verificada.
- **Rama**: development

## Verificado O0

- `TL-LANG-517` (CL-LANG-020, Host.Language, `nsc_component_tests`): PASS — `BlockAllocator` +
  `new (alloc) A(42)`, `stackalloc int[2]`, `int[]`, `AsyncIterator` con `yield` + `await` → exit 42.

## Cobertura de las cuatro aristas

| arista | evidencia |
|---|---|
| allocators | `BlockAllocator : IAllocator` + `new (alloc) A(42)` |
| generators | `AsyncIterator<int,int>` + `yield`/`await MoveNextAsync` |
| containers | `int[]` (contenedor productivo) |
| noheap | `stackalloc int[2]` (reserva sin heap) |

## Contexto

- El hueco era probar la combinación. Las aristas ya tenían pruebas individuales (allocator
  `TL-LANG-477`, generator `TL-LANG-455`, noheap `TL-LANG-333`, container Core); aquí se cerró la
  combinación productiva end-to-end.
