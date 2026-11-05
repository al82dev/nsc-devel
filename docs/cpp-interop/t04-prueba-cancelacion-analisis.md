# Cancelación durante el análisis Clang

Se añade una prueba determinista que solicita cancelar cuando Clang consulta un
encabezado real durante el análisis. Comprueba que se alcanzó ese punto, que el
token quedó cancelado y que el análisis no entrega un AST libre de errores.

La prueba se ejecutará junto con las demás comprobaciones de la tanda. No afirma
que se interrumpan instrucciones de CPU ni lecturas que ya estaban en curso.
