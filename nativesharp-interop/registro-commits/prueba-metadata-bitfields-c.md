# Prueba de campos de bits C precompilados

El productor publica punteros a structs C ordinarios y empaquetados. Dos consumidores verifican lectura, escritura y comprobación desde C: uno vuelve a importar el encabezado y el otro usa exclusivamente la biblioteca precompilada. El script contempla O0–O3; la creación del fixture no demuestra que haya pasado.

El primer intento del consumidor sin encabezado usaba los nombres C directamente y falló por identificadores desconocidos. El fixture ahora obtiene sus tipos de los valores retornados por la API publicada, igual que el caso existente de metadata; esa prueba no decide todavía la visibilidad de nombres C sin importar el encabezado.
