# Tanda 08: dimensiones de campos C importados

La primera validacion rechazo campos compuestos porque el modelo solo calculaba
el ancho de escalares y punteros. Se transportan tamaño y alineacion observados
por Clang para validar campos de arreglos, enum y record; la visibilidad analiza
el elemento de arreglos. La fase de layout posterior sigue validando el tipo.
Falta repetir la bateria y no se habilitan campos empaquetados sin comprobar.
