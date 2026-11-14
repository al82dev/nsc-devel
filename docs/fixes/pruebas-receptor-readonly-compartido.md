# Pruebas de solo lectura de punteros compartidos tras b6b2c3108 (NSC-NEW-006, NSC-NEW-007)

`a6a237e22` (2026-11-11) y `b6b2c3108` (2026-11-13) declararon `readonly` los miembros públicos de
`Rc`, `Arc` y sus handles: `Clone`, `Downgrade`, `Borrow` y las consultas solo actualizan contadores
del bloque compartido o leen el handle. Dos pruebas anteriores codificaban la conducta previa y
fallaban con el compilador vigente.

## `nsc.sdk.shared-readonly-borrow` (NSC-NEW-007)

El caso negativo esperaba que `input.Clone()` sobre `in Rc<Payload>` se rechazara con "readonly
receiver cannot invoke mutating struct member 'Clone'". `Clone` es `readonly` ahora y ningún miembro
público muta el dueño, así que ninguna llamada a miembro puede mostrar que el dueño recibido por
`in` sigue inmutable. Lo muestra reemplazarlo: `input = SharedPointers.MakeRc<Payload>()` se rechaza
por `in` con "left side of assignment is not assignable", y la misma sentencia con un parámetro por
valor se acepta como control. Se comprueba para Rc y Arc.

## `nsc.compiler.readonly-borrowed-array`, caso `struct_payload_child_mutation` (NSC-NEW-006)

`borrowed Child saved = boxOwner.Get().Item; saved.Value = 10;` esperaba "borrowed local requires a
direct live owner". El enlace prestado desde la carga de un dueño compartido se acepta ahora; lo que
debe fallar es la escritura, y falla con "left side of assignment is not assignable", igual que
`struct_payload_child_write`. La expectativa pasa a ese texto. Los demás casos no cambian.

Resultado: las dos pruebas pasan.
