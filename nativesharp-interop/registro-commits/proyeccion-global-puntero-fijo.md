# Proyección de arreglo C con punteros constantes

La prueba produjo `C/C++ global type is not yet mapped` al importar
`int * const[2]`. La proyección reutiliza el transporte readonly del arreglo
para el calificador del slot, manteniendo el tipo apuntado independiente. Solo
se admite la forma C y se conserva el rechazo de volatile o calificadores
anidados que no puedan representarse. Falta construir y validar los escapes.

La construcción incremental `f3f7271196c4666b` pasó la prueba positiva en
O0–O3, comprobada desde C, y rechazó reasignación del slot, escape por dirección
y paso a una función que recibe `int**`. La suite de sesión pasó 1/1. Hubo dos
advertencias de inicializadores en `CppSemanticInventory.cpp`. No cubre otras
combinaciones de punteros y calificadores ni cierra la tanda.
