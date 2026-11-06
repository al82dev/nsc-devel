# Procedencia del buffer de records

La sesión Clang captura ruta física, offset y SHA-256 del buffer realmente
analizado para records con ubicación de archivo directa. Calcula la huella una
vez por archivo durante cada recorrido; no relee el archivo ni atribuye una
ubicación física directa a declaraciones generadas mediante macros.

La información es transitoria y todavía no se serializa ni utiliza para unir
identidades. Se requieren controles y construcción incremental antes de usarla.
