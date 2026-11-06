# Procedencia del buffer de records

La sesión Clang captura nombre de archivo, offset y SHA-256 del buffer realmente
analizado para records con ubicación de archivo directa. Calcula la huella una
vez por archivo durante cada recorrido; no relee el archivo ni atribuye una
ubicación directa a declaraciones generadas mediante macros. La ruta puede ser
virtual y por sí sola no prueba procedencia física ni identidad nominal.

La construcción incremental de la suite y su ejecución terminaron con código 0.
La información es transitoria y todavía no se serializa ni utiliza para unir
identidades. El caso de encabezado compartido sigue pendiente.
