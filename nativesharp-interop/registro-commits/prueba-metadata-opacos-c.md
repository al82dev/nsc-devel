# Punteros C opacos en superficie precompilada

Se prepara un productor NativeSharp que publica métodos con punteros a struct y
union C incompletos, y un consumidor que usa esos métodos sin importar el
header. Se verifican producción de metadata, enlace y ejecución O0–O3. El
fixture aún no se ha ejecutado y no demuestra compatibilidad precompilada.

Con build `6b5448c0a735018f`, el productor y el consumidor compilaron,
enlazaron y ejecutaron en O0–O3 para ambos tipos. El consumidor utilizó
variables inferidas y no importó el header C directamente. La misma ejecución
incluyó los controles por valor y el consumidor directo. No acredita todas las
formas de nombres, calificadores o conciliación con otro header.
