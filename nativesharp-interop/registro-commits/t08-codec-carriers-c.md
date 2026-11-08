# Sobre preparado para carriers anónimos C

El codec interno dispone de funciones para escribir y leer un sobre versión 6:
contiene un catálogo anterior completo y una lista canónica de índices de campos
que son carriers. Valida límites, duplicados, nombre interno y tipo; el lector
reconstruye en temporales y comprueba la codificación canónica antes de publicar
el resultado. Los catálogos sin carriers no necesitan cambiar de representación.

Una prueba preparada usa los records de Clang con dos niveles para comprobar el
round trip de tres carriers y el rechazo de bytes sobrantes.

Las funciones todavía no están conectadas al despacho ni a la publicación de
NSMETA. Falta integrar versión, productores, consumidores y pruebas de formato y
compatibilidad. No se ha compilado ni probado este cambio; la tanda 8 sigue abierta.
