# Enum C compartido

Se prepara una prueba donde el productor NativeSharp y el consumidor importan
el mismo enum C; se comparan valores procedentes de ambas funciones y un
enumerador. La primera ejecución no llegó al compilador del consumidor porque
el script lo invocó antes de construir el productor. Se corrigió el orden y
se repitió la batería. El caso O0 detectó rechazo incorrecto de locales de enum
inferidos y ambigüedad del enumerador; la batería terminó con código 1. Se
prepararon correcciones. La repetición verificó que los valores enum de la
biblioteca y el encabezado se comparan y que el enumerador se resuelve de O0
a O3. La comparación directa del enum con su enumerador entero sigue fallando
y se conserva como prueba independiente.
