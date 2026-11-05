# Lectura de nombres DLL en bibliotecas COFF

El verificador inspecciona entradas de importación COFF para obtener los nombres
de DLL asociados a una biblioteca. Comprueba límites de archivo y distingue las
entradas cortas de algunos descriptores largos; rechaza los casos que requieren
soporte de enlace largo todavía no implementado.

El lector sirve para validar referencias de ejecución. No se considera soporte
de todos los formatos COFF ni sustituye las pruebas de enlace.
