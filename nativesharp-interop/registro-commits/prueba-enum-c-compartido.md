# Enum C compartido

Se prepara una prueba donde el productor NativeSharp y el consumidor importan
el mismo enum C; se comparan valores procedentes de ambas funciones y un
enumerador. La primera ejecución no llegó al compilador del consumidor porque
el script lo invocó antes de construir el productor. Se corrigió el orden y
falta repetir la batería de O0 a O3.
