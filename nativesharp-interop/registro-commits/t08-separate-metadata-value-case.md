# Tanda 08: separar la prueba de record por valor

La primera ejecución se detuvo en O0 porque el importador no proyecta todavía
el retorno C de record por valor. Se conserva ese caso como variante independiente
después de la matriz de enum para observar ambos contratos por separado; el fallo
de la variante continúa siendo parte de la validación y no se considera resuelto.
