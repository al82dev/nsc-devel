# Cancelación en accesos a archivos de Clang

Una capa privada comprueba el token antes de consultar o abrir archivos, enumerar
directorios y resolver rutas. Cuando la API admite un error, devuelve
`operation_canceled`; la consulta booleana de existencia devuelve falso.

Esto habilita puntos de interrupción durante la E/S. No interrumpe una operación
ya iniciada ni promete detener de inmediato el procesamiento que solo use CPU.
