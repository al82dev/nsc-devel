# Productor que importa C en una ruta Unicode

Se corrigieron conversiones de ruta de proyecto y de ubicaciones importadas de C
en el driver y la colección semántica. El grafo de bibliotecas precompiladas
conserva la ruta de cada módulo como UTF-8.

El control construye en O0 un productor con proyecto, fuente, encabezado y DLL
C dentro de una carpeta que incluye U+6F22. El consumidor usa la biblioteca
producida después de retirar el encabezado y se ejecuta desde otra carpeta.
La prueba dirigida terminó con código 0; también pasaron sus controles C, TLS
y de referencias Unicode. Se agregó al validador del bloque de identidad.

La construcción incremental terminó correctamente con advertencias de campos
no inicializados en los archivos de importación C. Esta prueba no demuestra
otros niveles de optimización ni cierra la tanda 8.
