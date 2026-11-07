# Herramienta ausente con caché existente

Se amplía el control de caché para retirar LLD del PATH después de generar un
ejecutable incremental válido. Una nueva construcción con LLD debe fallar en
vez de reutilizar el producto anterior, y el archivo previo debe permanecer
idéntico. El control del destino inicialmente inexistente se conserva.
La prueba ampliada todavía no se ha ejecutado.
