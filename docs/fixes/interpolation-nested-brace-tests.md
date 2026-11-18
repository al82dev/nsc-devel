# Llaves anidadas en huecos de interpolación

LKF-425: se añadieron controles léxicos para llaves anidadas dentro de huecos en las
formas normal y raw. Ambos conservan exactamente el contenido del hueco sin diagnóstico.
La prueba no afirma que la expresión de ejemplo sea semánticamente válida.

La construcción incremental de `nsc_lexer_tests` pasó 59 comprobaciones. Los controles
de llave de cierre aislada y llave escapada siguen incluidos en el mismo blanco.
