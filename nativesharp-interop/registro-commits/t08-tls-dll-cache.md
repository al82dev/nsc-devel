# Tanda 8: caché del consumidor de dos DLL TLS

El fixture compila un consumidor sin encabezados propios contra dos productores precompilados. El consumidor lee y modifica de manera independiente las variables TLS de las dos DLL. El primer build publica el ejecutable y ambas DLL en una carpeta aislada; el script los retira y compila otra vez con la misma caché.

`validar-cache.cmd` terminó con código 0: la segunda compilación informó `compilation-bundle-cache: hit`, restauró los tres archivos y el ejecutable volvió a pasar. El informe de contadores de objetos que aparece después procede del resultado guardado del build inicial y no se usa como indicador del hit. El fixture emplea O0 y productores construidos sin caché; no prueba caché con encabezados C/C++ en el proyecto consumidor ni primeros accesos concurrentes.
