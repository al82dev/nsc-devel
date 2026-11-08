# T08 H3: reconstrucción de aliases C guardados

La reconstrucción recibe el proyecto productor y el índice de referencia
resuelto por su llamador, comprueba módulo, destino y USR, y conserva el tipo
capturado sin transportar la ubicación original. Un fallo deja la salida intacta.
La operación no instala el alias en el parser ni resuelve el índice de referencia.

El control toma un typedef de Clang real, lo captura, codifica, decodifica y
reconstruye; compara su USR y tipo canónico y comprueba el rechazo de un proyecto
inválido. La construcción incremental y el control conjunto con la reproyección
de globales terminaron con código 0. No se probó la ruta de un NSMETA publicado
ni se ejecutó la batería completa de la tanda 8.
