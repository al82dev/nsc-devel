# NS304-T13-F06 — Trailing/append storage

- **Estado**: IMPLEMENTADO (construcción + header + acceso + bounds contra `Length`)
- **Rama**: development

## Resultado (2026-10-14)

- **Surface**: `struct Packet { int Length; int Capacity; [TrailingStorage(nameof(Length), nameof(Capacity))] byte[*] Data; }`
  y `Packet p = new (allocator, trailing: capacity) Packet(length, capacity)`.
- **Parser**: `nameof(X)` como argumento de atributo constante; atributo `TrailingStorage` (target `Field`).
- **Sema**: `new (alloc, trailing: N)` excluye `N` de los argumentos del constructor; el header struct se
  expone **por valor**.
- **Layout**: `align_up(sizeof(header), alignof(T)) + Capacity*sizeof(T)`; el campo `T[*]` (size 0) queda al
  final sin alterar los offsets del header.
- **HIR**: el atributo `[TrailingStorage]` se resuelve (struct → `Length` → offset) y se baja al
  `RawElementAddress` como presencia+valor (`hasTrailingLengthBounds` + offset relativo al campo flexible).
- **LLVM**: `RawElementAddress` flexible emite `index < Length` → trap `BoundsViolation` (exit 132).

## Causa raíz (bounds)

El `RawElementAddress` tiene como base la **dirección del campo flexible `Data`** (offset 8), no el inicio
del struct. El offset de `Length` resuelto por el layout es relativo al **struct** (0), por lo que había que
calcular el offset **relativo al campo flexible** (`lengthOffset - dataOffset = -8`). Además, un centinela
numérico (`!= 0`) no servía porque `Length` está legalmente en offset 0; se separó presencia de valor
(`hasTrailingLengthBounds` + `trailingLengthRelativeOffset`).

## Verificación O0

- `TL-LANG-488`: `Packet(4,8)` → `Length==4`, `Capacity==8`, `Data[0]`/`Data[3]` (dentro de `Length`) correctos (exit 0).
- `TL-LANG-489`: `Data[4]` (== `Length`) trapea `BoundsViolation` (exit 132).
- `TL-LANG-490`: `Packet(2,8)` → `Data[5]` trapea aunque `5 < Capacity` (el guard usa `Length`, no `Capacity`).
- Guards: 484/485/486/487 PASS.

## Commits

- `fb721238` nameof + TrailingStorage; `fc85f3e2` parser `trailing:`; `b022ed64`/`fd3ec93d`/`5debc348`
  arity + lowering; `1db4932d`/`955df72d`/`f4eea0d3` header por valor + extensión; `cdc455d8` bounds guard.
