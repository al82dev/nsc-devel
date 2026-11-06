# Orden MS para elementos de arreglos globales C volatile int

El lowering comprueba que la dirección de un elemento int provenga de un
global C marcado con modo MS y alineación acreditada, y que cada stride sea
múltiplo de cuatro. Solo entonces emite acquire para lectura y release para
escritura, incluidas las dos operaciones de una asignación compuesta. Otros
tipos, campos y punteros arbitrarios no siguen esta ruta.

El código está preparado; faltan los controles de IR y la validación agrupada.
