# Publicación protegida de archivos de ejecución

El publicador puede conservar un archivo de ejecución existente si su contenido
coincide con el archivo preparado. Si es distinto, informa el conflicto; si no
existe, usa la operación transaccional que impide reemplazar un destino creado
entre la comprobación y la instalación.

Este cambio se valida junto con el resto de la publicación y del empaquetado.
