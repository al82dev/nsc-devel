# Tanda 32 — SH30 — commit

**Estado:** gate dirigido aprobado; listo para commit.

`selfhost(tanda-32): complete the semantic object model`

SH30 binds the owning type while analyzing methods and constructors, gives `this` and `base`
explicit typed lvalue facts, and resolves implicit member names against the current type. Its gate
locks fields, constructors, properties, indexers and operators byte-for-byte across O0–O3 and
executes a type-owned static method through LLD and NSLink on all eight routes. Instance functions
are deliberately rejected by the scalar backend admission instead of being miscompiled without a
receiver.

No entran objetos, ejecutables, logs, response files ni `selfhost/out`.
