# T08: dirección de elemento flexible empaquetado

`&value->data[0]` sobre `FlexPacked` (`pack(1)`, `int data[]`) fue aceptado
por `--check-project-method-bodies`, aunque el puntero `int*` perdería la
alineación reducida. El código ya protegía arreglos fijos, pero no flexibles.

La comprobación de alineación del miembro se extiende al índice flexible y
examina su tipo de elemento. Las lecturas y escrituras indexadas siguen usando
su alineación propia; falta ejecutar las pruebas dirigidas y las regresiones.
