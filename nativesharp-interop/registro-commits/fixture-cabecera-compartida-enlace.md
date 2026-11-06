# Enlace de la prueba de encabezado compartido

El consumidor analiza `Api.h` sin volver a añadir `api.lib`: el objeto C ya
está incluido en la biblioteca NativeSharp productora. Así la prueba ejercita
la misma declaración sin presentar el mismo objeto dos veces al enlazador.
El caso completo aún no se ha validado con la reconciliación conectada.
