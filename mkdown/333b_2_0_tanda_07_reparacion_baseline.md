# Tanda 7 — Reparación del baseline / Batch 7 baseline repair

## Hallazgo / Finding

El baseline final de la tanda 7 encontró que el ítem 329 había endurecido correctamente el
operador `&` para rechazar bitfields, pero dejó implícita la direccionabilidad de los elementos de
arreglos nativos. Esos elementos seguían siendo asignables, aunque el nuevo chequeo exige además
que el operando esté marcado como almacenamiento direccionable. Esto rompía usos históricos como
`&bytes[0]` en `CString` y `Span<T>`.

The final batch 7 baseline found that item 329 correctly tightened `&` to reject bit fields, but
left native-array element addressability implicit. Those elements remained assignable, while the
new guard also requires the operand to be marked as addressable storage. This regressed established
uses such as `&bytes[0]` in `CString` and `Span<T>`.

## Reparación y prueba / Repair and test

El análisis de `array[index]` vuelve a declarar explícitamente que el resultado representa
almacenamiento mutable y direccionable. La prueba dirigida del ítem 329 conserva las dos mitades
del contrato: `&value->bitfield` permanece rechazado y `&array[index]` vuelve a compilar.

The `array[index]` analysis now explicitly records mutable addressable storage. Item 329's focused
test preserves both halves of the contract: `&value->bitfield` remains rejected and
`&array[index]` compiles again.

No cambia ningún formato persistente ni versión de contrato. / No persistent format or contract
version changes.
