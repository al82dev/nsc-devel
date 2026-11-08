# Proyecto y fuente Unicode con bibliotecas C

Se corrigieron conversiones de rutas en el registro de proyectos implícitos,
los comandos de comprobación, el almacenamiento del nombre de fuente, la
resolución semántica y el índice de fuentes de la caché. Los diagnósticos de
colisiones de DLL también conservan rutas UTF-8.

Validación: construcción incremental de NSC y `tls-dll-unicode\validar.cmd
--cache --unicode-source`, con resultado 0. El caso compila y ejecuta en O0 un
consumidor cuyo proyecto, fuente y directorio de salida incluyen U+6F22;
verifica una restauración desde caché después de retirar los productos y
confirma que una DLL preexistente diferente se conserva y se diagnostica sin
excepción interna. Los controles C y TLS de dos DLL también pasaron.

El validador del bloque incluye ahora este caso. Las variantes adicionales de
rutas y las equivalencias Unicode de mayúsculas y minúsculas siguen abiertas;
esta evidencia no cierra el bloque ni la tanda 8.
