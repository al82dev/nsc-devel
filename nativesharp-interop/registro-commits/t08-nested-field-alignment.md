# T08-010 / T08-012: alineación de campos anidados

El MIR de `parent->child.value` contiene dos FieldAddress: la alineación
nominal de `value` puede exceder la garantizada por `child` en un record
empaquetado. El cálculo de alineación sigue ahora direcciones de campos y
elementos de arreglo por valor, combinando alineaciones, offsets y strides.
Se detiene en un valor cargado para no confundir la alineación de un slot
puntero con la del objeto apuntado. Cargas y escrituras directas utilizan el
resultado. No habilita todavía records anidados subalineados ni acredita otras
rutas; falta la prueba agrupada.
