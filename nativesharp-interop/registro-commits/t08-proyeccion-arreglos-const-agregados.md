# T08-015 y T08-016: proyección de arreglos C const

La ruta de arreglos de enums y records acumula const de sus dimensiones y del
elemento y lo conserva como readonly del campo. El mapeo enum utiliza una copia
sin const para verificar identidad y layout; volatile sigue rechazado. La ruta
de arreglos puntero permanece sin cambios. Falta validación de lectura y rechazo.
