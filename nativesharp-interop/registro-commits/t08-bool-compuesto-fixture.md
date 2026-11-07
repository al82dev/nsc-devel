# T08-015: fixture de bit a bit compuesto sobre `_Bool` C

El caso previamente negativo ahora comprueba `|=`, `&=` y `^=` sobre un
elemento C `volatile _Bool`, verificando los valores intermedios en O0–O3.
El fixture solicita además IR y comprueba la presencia de una carga y una
escritura `atomic volatile i8`. Se conserva el control negativo separado de
operación compuesta sobre bool nativo.

El código y el fixture están preparados; no se ha reconstruido ni ejecutado
esta versión. El resultado y los órdenes acquire/release concretos deberán
comprobarse antes de acreditar el caso. No cierra la tanda ni otras operaciones.
