# IR del puntero global volatile

Los verificadores exigen que C y NativeSharp generen lectura acquire y escritura
release del almacenamiento del puntero, con alineación de ocho bytes. Clang
puede representar el valor atómico como puntero o entero de 64 bits. La prueba
ya existente continúa exigiendo escritura ordinaria en el objeto apuntado.
El resultado se determinará con la batería agrupada.
