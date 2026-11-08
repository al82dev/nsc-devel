# T08: alinear la conversión implícita de miembro flexible

La conversión `int* first = value->data` fue aceptada para `FlexPacked` con
`pack(1)`: el análisis del campo comprobaba la alineación del tipo `int[*]`,
que no se reconocía como escalar. Se usa el tipo del elemento para la misma
comprobación aplicada a arreglos fijos.

La corrección evita aprobar esa conversión mientras no exista un contrato de
puntero subalineado adecuado. Falta compilar y ejecutar el control dirigido.
