# T08-014 / T08-016: arreglos C empaquetados de punteros

La proyección permite un arreglo fijo C subalineado de punteros cuando el
mapeador existente puede representar el tipo sin descartar calificadores.
La alineación del slot se deriva del campo y los strides; la dirección del
slot se mantiene protegida por el análisis de índices. Las restricciones del
mapeador para apuntados calificados y tipos no representables permanecen.
La prueba mixta está pendiente.
