# NS304-T06-F05 — Items por value, move, ref y ref mut con lifetime asociado

- **Estado**: HECHO (surface `Iterator<ref T>`/`Iterator<ref mut T>` + item por referencia; rechazo borrow-por-MoveNext = refinamiento)
- **Rama**: development

## Resultado (item ref/ref mut — 2026-10-14)

- **Superficie (#2 FINAL)**: `Iterator<ref T>` → `iterator:ref:<T>`; `Iterator<ref mut T>` → `iterator:refmut:<T>`
  (`TypeSyntax.ReferenceQualifierSyntaxKind` + parser + `canonicalTypeKey`).
- **Item por referencia**: `yield return x` guarda `&x` (dirección del local en el frame) en el slot
  del item; `Current` devuelve la referencia (puntero). Layout `ref:<T>` = 8 bytes (puntero).
- **foreach**: `foreach (ref T v in ...)` / `foreach (ref mut T v in ...)` bindea el item por
  referencia (escribe a través de él).
- **Pruebas O0**: `TL-LANG-440` (ref item, exit 10) + `TL-LANG-441` (ref mut write, exit 42) PASS.
  Guards `TL-LANG-439/382/383` PASS. Commit `2a598bf4`.

## Residual (refinamiento)

El **rechazo** del item prestado que sobrevive a `MoveNext`/destrucción (plan #3): hoy
`ref int v = ref it.Current; it.MoveNext(); return v;` compila (el frame persiste). Requiere modelar
`it.MoveNext()`/`it.Drop()` como invalidación del borrow del item (análisis de dataflow sobre el
borrow origin). Es una mejora de seguridad (rechazo causal), no un hueco funcional.
