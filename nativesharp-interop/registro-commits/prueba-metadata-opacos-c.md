# Punteros C opacos en superficie precompilada

Se prepara un productor NativeSharp que publica métodos con punteros a struct y
union C incompletos, y un consumidor que usa esos métodos sin importar el
header. Se verifican producción de metadata, enlace y ejecución O0–O3. El
fixture aún no se ha ejecutado y no demuestra compatibilidad precompilada.
