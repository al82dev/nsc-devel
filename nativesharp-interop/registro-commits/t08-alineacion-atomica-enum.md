# T08-015: alineación comprobada para enum volatile

El cálculo exige identificación MIR, modo MSVolatile efectivo y una garantía
de alineación de la dirección no menor que el ancho del enum. Sin esa prueba
no autoriza un acceso atómico. Falta conectar el resultado al emisor y validar.
