# Proyección de callback C volatile

El mapeo estructural conserva ahora el calificador volatile del almacenamiento
al proyectar punteros a función C. Se preparan por separado el orden y la
alineación necesarios para Windows; este cambio aislado no acredita el acceso.
