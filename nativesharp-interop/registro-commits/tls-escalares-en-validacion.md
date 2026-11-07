# TLS escalar en la validación de tanda 8

Se añade el caso de escalares al validador agrupado, una comprobación del IR
de accesos `volatile` y un caso negativo de escritura `const`. Se verifican
tras reconstruir el compilador; su presencia no implica cierre de la tanda.
