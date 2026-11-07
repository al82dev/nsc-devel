# Herramienta ausente con caché existente

Se amplía el control de caché para retirar LLD del PATH después de generar un
ejecutable incremental válido. Una nueva construcción con LLD debe fallar en
vez de reutilizar el producto anterior, y el archivo previo debe permanecer
idéntico. El control del destino inicialmente inexistente se conserva.
La prueba ampliada pasó con NSC build `b910fa13ea6d5855`: informó la
ausencia, no generó el destino nuevo y conservó el archivo previo sin cambios.
