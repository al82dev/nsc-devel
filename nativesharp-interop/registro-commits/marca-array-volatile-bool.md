# Marca de arreglo global C volatile _Bool

La raíz HIR conserva una garantía de un byte para el arreglo bool C con modo
MS. Una comprobación separada de los demás escalares exige el tipo, la raíz y
la marca antes de habilitar el tratamiento de almacenamiento. La emisión y
validación agrupada todavía están pendientes.
