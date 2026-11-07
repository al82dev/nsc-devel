# Prueba de la barrera provisional de TLS en DLL

El control utiliza el proyecto directo existente y exige el diagnóstico
específico antes de generar código, en vez de aceptar un fallo posterior por
`_tls_index`. La prueba positiva de TLS estático se conserva por separado.
La ejecución está pendiente de la construcción incremental agrupada. Esta
prueba no sustituye el caso positivo de DLL ni valida la tanda.
