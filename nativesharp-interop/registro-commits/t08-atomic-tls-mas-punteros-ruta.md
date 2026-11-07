# T08: argumentos de la generación del fixture TLS

La primera ejecución falló antes de llamar al compilador porque la ruta del
directorio origen terminaba en barra invertida antes de la comilla y Python no
recibió el segundo argumento. Se agrega un punto final a esa ruta. La ejecución
corregida sigue pendiente y el fallo no acredita comportamiento de NSC.
