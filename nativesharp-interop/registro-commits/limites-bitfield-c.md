# Límites de acceso a campos de bits C

La ventana de lectura y escritura respeta la alineación natural del tipo y rechaza solapamientos con campos de bits de otra secuencia, separada por un campo ordinario o de anchura cero. Esto evita modificar una ubicación de memoria independiente. La validación de ejecución se hace junto al resto de la tanda 08.
