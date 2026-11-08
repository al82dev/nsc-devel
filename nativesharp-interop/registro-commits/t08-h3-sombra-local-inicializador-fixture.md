# T08 H3: llamada C en inicializador local

El caso de ámbito añade un bloque donde una variable local se inicializa con
una llamada a la función C homónima. El resultado esperado total pasa a 264.
El control distingue la referencia previa a la declaración del uso de la
variable ya declarada; su resultado se registrará tras ejecutarlo.
