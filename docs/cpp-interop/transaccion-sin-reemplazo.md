# Publicación de artefactos sin reemplazo

Se agrega una opción a la transacción de artefactos para instalar un archivo
preparado solamente si el destino no existe. El indicador viaja con cada
entrada; la implementación usa una operación sin reemplazo en Windows y un
enlace duro en la otra rama de la plataforma.

El cambio permite proteger un destino existente durante la publicación. Esta
revisión registra el código acumulado; no afirma una validación aislada del
commit. Su verificación forma parte de las pruebas agrupadas del conjunto.
