# T08: control de identidad MIR para bool C volatile

El test de identidad añade un estado de namespace y compara las identidades
generadas al cambiar el indicador de almacenamiento C volatile bool. Comprueba
además que el índice de identidad coincide con la identidad calculada después
del cambio. Así se verifica la distinción que evita reutilizar código generado
con una declaración física distinta.

El control está preparado y sin construir ni ejecutar; no implica por sí solo
que la caché de artefactos esté habilitada para proyectos con headers C.
