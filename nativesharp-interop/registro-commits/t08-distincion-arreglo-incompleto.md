# Tanda 08: distinguir arreglo incompleto

El snapshot conserva si Clang clasifica el arreglo como incompleto, separado de
otros limites no constantes y de un limite cero. La comparacion de miembros
retiene esa diferencia. Falta proyectar su identidad y validar su uso, sin
confundirlo con un arreglo flexible de struct ni asignarle tamaño cero.
