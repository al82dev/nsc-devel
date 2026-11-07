# T08-004: origen de global TLS interna con referencia DLL

El control de IR mostro dos globales internas TLS declaradas `dllimport` y
accedidas por la ruta TLS de DLL, aunque pertenecen al objeto generado para la
sesion del encabezado. Se excluyen las candidatas internas de la inferencia de
origen DLL, conservando esa inferencia para las globales externas. Falta
construir y validar el conjunto.
