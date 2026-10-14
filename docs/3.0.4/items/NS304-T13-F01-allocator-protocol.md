# NS304-T13-F01 — Protocolo allocator

- **Estado**: HECHO (trait `IAllocator` + firma/efectos congelados + allocator mínimo de usuario probado)
- **Rama**: development

## Resultado (2026-10-14)

- **Trait**: `NativeSharp.Core.IAllocator` (surface #9) con `byte* Allocate(int)`, `byte* Resize(byte*,int,int)`,
  `void Deallocate(byte*)`. Firma y efectos congelados en el comentario del trait: `bytes<=0`/fallo → null;
  Resize conserva `min(old,new)` y null deja `ptr` válido; pairing = un Deallocate por Allocate, Resize
  sustituye el puntero.
- **Prueba O0** (`TL-LANG-432`, exit 0): un `BlockAllocator` de usuario (bloques fijos sobre `byte[]`)
  implementa el trait y se verifica allocate→write→resize(conserva)→deallocate→reuse + los fallos
  (tamaño 0, sobre-capacidad, resize que no cabe).

## Residual (F02, no F01)

- La integración lingüística (elegir el allocator en `new`/construcción) y el contrato de provenance
  (F07) son los verticales siguientes; F01 solo congeló el protocolo.
