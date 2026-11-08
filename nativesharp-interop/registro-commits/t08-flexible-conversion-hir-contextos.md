# T08: selección contextual de HIR para conversión flexible

HIR vuelve a reconocer la marca y categoría de ubicación de la expresión para
reconstruir la conversión flexible en inicializadores, asignaciones y retornos.
Para los demás casos conserva el clasificador anterior y su constante entera.
Las llamadas usan su selección semántica. La selección semántica y las pruebas
de estos contextos aún no están conectadas ni ejecutadas.
