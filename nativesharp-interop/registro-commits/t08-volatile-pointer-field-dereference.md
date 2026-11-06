# Desreferenciación de un campo puntero volatile

La prueba de un struct volatile mostró que la carga volatile de un campo puntero
se confundía con el calificador del objeto apuntado al escribir mediante `*`.
HIR separa ahora el valor del puntero de la dirección de destino mediante un
nodo transparente. La carga conserva volatile; la escritura apuntada no lo
hereda. La batería debe validar este caso y las regresiones antes del cierre.
