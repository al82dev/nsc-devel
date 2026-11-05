# Tanda 08: dimensiones del tipo enum

La captura consulta tamaño y alineación del tipo enum en Clang cuando el tipo
no es incompleto ni dependiente. La ausencia de dimensiones permanece explícita;
no se asigna el tamaño de un entero por defecto. Falta conectar este dato a la
importación de tipo y comprobarlo en la validación de la tanda.
