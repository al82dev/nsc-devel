# Control precompilado sin TLS

La prueba transitiva incluye una biblioteca C estática sin TLS publicada por
una biblioteca NativeSharp precompilada. El consumidor comprueba ejecución y
selección automática de NSLink, para evitar que la consulta de metadata fuerce
LLD en toda biblioteca precompilada. Pendiente de la validación agrupada.
