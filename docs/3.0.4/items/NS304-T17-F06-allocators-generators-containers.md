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

## Nota de reemplazo — Congelamiento 3.0 (DEC-09, C-08)

La firma de `IAllocator` que describe este documento fue reemplazada en 3.0, y la nueva es la única que se acepta:

```
byte* Allocate(long bytes, int alignment);
byte* Resize(byte* ptr, long oldBytes, long newBytes, int alignment);
void Deallocate(byte* ptr, long bytes, int alignment);
```

El `BlockAllocator` de `TL-LANG-517` implementa la firma nueva.

Una implementación con la firma anterior (`Allocate(int)`, `Resize(byte*, int, int)`, `Deallocate(byte*)`) recibe
en ese miembro un diagnóstico que nombra la firma nueva
(`docs/3.0/decisiones-congelamiento/32-P6-IALLOCATOR-FIRMA-UNICA.md`).
El texto de arriba se conserva como registro histórico de 3.0.4.
