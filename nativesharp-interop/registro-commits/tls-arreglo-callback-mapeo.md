# Mapeo de arreglo C de callbacks

Tras habilitar la proyección TLS, `int*[2]` ya no produce diagnóstico, pero
NSC `e412549986d53ba2` rechaza el arreglo de callbacks porque `mapGlobalType`
no acepta ese elemento en su ruta de arreglos. Se reutiliza el proyector
existente de punteros a función C para decidir si el elemento y su firma son
representables. Los demás elementos no mapeados siguen rechazados. Pendiente
construcción y pruebas agrupadas, incluidas regresiones no TLS.
