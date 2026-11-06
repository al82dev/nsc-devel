# Enlace de la prueba de encabezado compartido

El consumidor analiza `Api.h` sin volver a añadir `api.lib`: el objeto C ya
está incluido en la biblioteca NativeSharp productora. Así la prueba ejercita
la misma declaración sin presentar el mismo objeto dos veces al enlazador.
Con la reconciliación conectada, el productor y consumidor compilaron,
enlazaron y ejecutaron correctamente de O0 a O3.
