# Proyección de función C sin prototipo

Una declaración C `f()` sin prototipo se proyecta como llamada de argumentos no fijados usando la ruta de promociones C, mientras `f(void)` conserva cero parámetros. Se limita a cdecl y declaraciones sin parámetros de definición antigua observados; otras formas requieren análisis. La validez ABI y los casos directos y negativos deben verificarse en conjunto.
