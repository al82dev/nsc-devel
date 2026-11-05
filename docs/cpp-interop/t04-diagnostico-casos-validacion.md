# Diagnóstico de casos nuevos de sesión

La primera ejecución de las pruebas nuevas reportó fallos en cancelación durante
la búsqueda de encabezados y en el control de invocación rechazada. La prueba de
cancelación ahora intercepta las operaciones de existencia, estado y apertura;
ambos casos informan sus valores observados si vuelven a fallar.

Se repetirá la batería completa de la tanda tras reconstruir el ejecutable de
pruebas. Este registro no afirma todavía que los fallos estén resueltos.
