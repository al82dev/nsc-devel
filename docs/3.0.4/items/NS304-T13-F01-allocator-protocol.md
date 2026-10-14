# NS304-T13-F01 — Protocolo allocator

- **Estado**: AUDITADO (Allocator/Arena son insumos; protocolo lingüístico sin congelar)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar protocolo allocator*.
Plan T13-F01 (DeepSeekDocs): *congelar firmas y efectos de allocate/resize/deallocate, alignment,
tamaño cero y failure; definir ownership del bloque y requisitos de pairing; probar allocator mínimo
de usuario antes de cambiar biblioteca estándar*.

Estado real:

- `lib/NativeSharp.Core/Allocator.ns`, `ArenaAllocator` y los modelos de construcción/destrucción
  existen como insumos, no como integración lingüística.
- No hay protocolo de allocator congelado (firmas + efectos + failure + pairing) ni el contrato de
  provenance (expresión, allocator, alignment, size, owner, failure, destrucción, deallocator).

## Gap

Falta congelar el protocolo allocator (allocate/resize/deallocate + alignment + tamaño cero +
failure + ownership/pairing) y el contrato de provenance que cruza HIR/MIR/NSMETA.

## Decisión de surface (material, para Alberto)

- **(A) protocolo `IAllocator`/`Allocator` con `Allocate/Resize/Deallocate`** (recomendada):
  el allocator es un protocolo explícito; cada allocation registra su provenance.
- **(B) allocator por atributo `[Allocator(...)]`** (análogo a T05-F08 arena).

Recomendación: **(A)**.

## Nota

F01 es la base de T13 (antes de F02 selección + F03 scope + F04 arena + F05 placement). Depende de la
surface de allocator (A/B) + el contrato de provenance. Es la pieza que desbloquea T06-F06 (pinning).
