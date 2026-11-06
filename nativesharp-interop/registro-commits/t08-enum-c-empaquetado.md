# T08-009 y T08-016: campos C enum empaquetados

Los campos C de enum y arreglos de enums pueden superar la barrera de
alineación reducida para llegar al mapeador existente, que verifica identidad,
tamaño y alineación contra Clang. Se mantiene el rechazo para C++ y para
calificadores que el mapeador aún no representa. Las pruebas y la validación
están pendientes; no se declara terminado el soporte de enum.
