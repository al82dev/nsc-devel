# T08 H3: nombres locales por desestructuración

El control `ee469d31b` reprodujo en O0–O3 la publicación indebida de la global
C `shadow_value`, aunque el consumidor ejecutó correctamente una plantilla que
declara ese nombre mediante desestructuración.

La detección de locales precedentes contempla ahora los identificadores de una
declaración de desestructuración, incluidos los patrones de tupla anidados, y
excluye el descarte `_`. El análisis solo considera declaraciones anteriores
en el bloque; el inicializador no queda cubierto por la declaración todavía.
La validación tras la construcción incremental se documentará por separado.
