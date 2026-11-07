# Controles de calificadores atómicos

El fixture prepara globales TLS `const _Atomic(int)` y
`volatile _Atomic(int)`, con lectura y actualización conjunta C/NativeSharp y
rechazo de escritura al valor const. La validación seguirá el resultado real;
no cubre otros tipos ni punteros atómicos.
