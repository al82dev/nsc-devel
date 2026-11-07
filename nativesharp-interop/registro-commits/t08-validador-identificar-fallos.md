# T08: identificación de casos fallidos en el validador

El gate acumulaba códigos de error, pero no imprimía qué script falló. `validar-enum-mixto.cmd` reprodujo código 1 sin mensaje explícito, de modo que contar solo las líneas con «fallo» de la ejecución integral omitía ese caso.

Se añade el nombre del caso que retorna error en las dos funciones que ejecutan scripts, conservando la acumulación y la salida final. No se cambia ningún resultado, prueba ni condición del gate. Se corrigió el registro anterior para distinguir cuatro fallos visibles del inventario aún no probado de todos los fallos. La tanda sigue abierta.
