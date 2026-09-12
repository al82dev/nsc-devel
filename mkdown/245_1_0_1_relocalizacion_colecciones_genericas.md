# 245 — Relocalización segura en colecciones genéricas / Safe generic collection relocation

**Línea / Line:** `1.0.1-dev`

## Resumen

Las colecciones que almacenan `owned T[]` ya no copian referencias durante shifts, swaps o
operaciones de rango. Cada cambio de ranura usa `move`, por lo que existe un solo owner durante
toda la operación y no únicamente al terminarla.

## Causa

El frontend endurecido detectó correctamente tres asignaciones inseguras al cerrar las plantillas
con referencias: el swap de `PriorityQueue<Payload, int>` y el desplazamiento de
`SortedList<int, string>`. El mismo patrón permanecía latente en miembros de `List<T>` que el
proyecto acumulativo no había demandado todavía. Se corrigió la familia completa para evitar que
el siguiente consumidor descubriera el mismo defecto método por método.

## Diseño

NativeSharp no puede declarar un local owner genérico que sea a la vez RAII para referencias y un
valor ordinario para structs/primitivos. Los swaps usan por ello un arreglo temporal de una ranura:

```nativesharp
const T[] temp = new T[1];
temp[0] = move(items[a]);
items[a] = move(items[b]);
items[b] = move(temp[0]);
delete[] temp;
```

El slot temporal termina vacío antes de liberar su bloque. Para un tipo valor, `move` se reduce a
una carga normal; para una referencia, cada fuente queda a null.

Los shifts recorren en la dirección que hace que el destino esté libre o ya vaciado por el paso
anterior. `AddRange` e `InsertRange` conservan prestado el bloque del arreglo recibido, pero
consumen explícitamente sus elementos: las ranuras referencia del llamador quedan a null y el
llamador sigue siendo responsable de `delete[]` sobre el bloque. Esta es una adaptación nativa,
no una simulación de la semántica GC de C#.

## Archivos

- `lib/NativeSharp.Core.Collections.Generic/PriorityQueue.ns`: swap de elementos y prioridades
  mediante slots temporales y transferencias explícitas.
- `lib/NativeSharp.Core.Collections.Generic/SortedList.ns`: shifts de inserción y eliminación con
  `move` para claves y valores.
- `lib/NativeSharp.Core.Collections.Generic/List.ns`: reemplazo, shifts, rangos y swap consistentes
  con el ownership endurecido.
- `examples/project_stage52/Main.ns`: demanda en O0 las rutas referencia de reverse, insert,
  remove y AddRange.
- `mkdown/245_1_0_1_relocalizacion_colecciones_genericas.md`: decisiones e historial.

## Evidencia dirigida

El contrato `nsc.stage52-1.generic-collections-additions` compila el proyecto real Stage 52 hasta
MIR/LLVM/COFF. La cobertura agregada obliga a materializar los miembros de `List<Payload>` que
antes permanecían perezosos, además de `PriorityQueue<Payload, int>` y
`SortedList<int, string>`. No se ejecuta baseline en esta entrega.

---

## English

Collections backed by `owned T[]` no longer copy references during shifts, swaps, or range
operations. Every slot-to-slot relocation is an explicit move, preserving exactly one owner
throughout the operation.

Because a generic local cannot uniformly be a RAII reference owner and an ordinary value, swaps
use a one-element native array as temporary owner storage. The temporary slot is moved out before
its block is deleted. Shifts run in the direction that makes each destination empty or vacated by
the preceding move.

`AddRange` and `InsertRange` keep borrowing the source array allocation but consume its elements:
reference slots become null while value elements behave as copies. The caller still releases the
array block. This is an explicit native ownership contract rather than emulated GC behavior.

The directed Stage 52 project now materializes reference-type List relocation/range members in
addition to the existing PriorityQueue and SortedList cases, and it reaches verified LLVM/COFF.
No baseline is run for this delivery.

## Commit de referencia / Reference commit

**Título ES:** Asegura la relocalización en colecciones genéricas

**Title EN:** Make generic collection relocation ownership-safe

**Descripción ES:** Sustituye copias internas por movimientos de ranura y documenta el consumo
de elementos en operaciones de rango para mantener un solo owner.

**Description EN:** Replace internal copies with slot moves and document range-element consumption
to preserve single ownership.
