# Importación directa y alias de DLL: control de identidad

Un ejecutable C importa `tls_read` desde `api.dll` y carga el mismo binario
como `alias.dll`. Compara la dirección de la función importada con la exportada
por el alias: son iguales únicamente si Windows utiliza el mismo módulo.

En el sistema probado, una copia del archivo produjo módulos distintos; un
hardlink produjo el mismo módulo. El control terminó con código 0 y se añadió
al fixture de DLL homónimas. El resultado explica por qué copiar ambas DLL con
nombres diferentes no basta cuando también hay una importación directa: podría
haber dos instancias y dos estados TLS.

Un hardlink por sí solo no resuelve la distribución: al copiar el paquete a
otro equipo puede convertirse en dos archivos independientes. La integración de
importaciones directas y transitivas sigue pendiente y no se marca como cubierta.
