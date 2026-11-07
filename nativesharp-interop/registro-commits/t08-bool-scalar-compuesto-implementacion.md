# T08: operación compuesta de `_Bool` C escalar

El analizador, HIR y LLVM preparan el uso de la ruta compuesta ya existente
para un global C escalar `volatile _Bool` en modo MS. La ruta conserva los
operandos enteros y flotantes, las promociones, el almacenamiento de byte y
los órdenes acquire/release. La condición excluye `_Atomic`, bool nativo y
C++; el arreglo previamente soportado mantiene su comprobación.

La implementación aún no se ha construido ni validado. Faltan controles
mixtos de ejecución e IR, incluidos los operadores y operandos admitidos.
No se modificó NSLink.
