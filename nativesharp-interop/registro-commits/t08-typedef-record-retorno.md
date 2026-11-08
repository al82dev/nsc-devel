# Retorno de record C por puntero a función

La prueba usa dos DLL con funciones variádicas homónimas y dos plantillas
precompiladas que asignan cada función a un typedef y reciben un `Payload` por
valor. Con dos productores, el typedef que nombraba al record también chocaba
con los records importados. Se extiende la comparación de cierres ya existente
a ese alias y se selecciona el record del productor, siempre que los candidatos
importados hayan sido comparados como equivalentes.

El retorno de un record de 4 bytes y el de uno de 24 bytes pasaron en O0–O3,
con controles C, ambos encabezados ocultos y restauración desde caché. En el
record grande se verifican los tres campos. El validador previo de records pasó,
incluido el rechazo de records incompatibles del mismo tamaño y de un encabezado
modificado. Los nuevos casos se agregaron a ese validador. No se ejecutó la
batería completa de la tanda ni se introdujeron versiones nuevas de NSMETA.
