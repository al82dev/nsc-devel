# Proyección de arreglo C con punteros constantes

La prueba produjo `C/C++ global type is not yet mapped` al importar
`int * const[2]`. La proyección reutiliza el transporte readonly del arreglo
para el calificador del slot, manteniendo el tipo apuntado independiente. Solo
se admite la forma C y se conserva el rechazo de volatile o calificadores
anidados que no puedan representarse. Falta construir y validar los escapes.
