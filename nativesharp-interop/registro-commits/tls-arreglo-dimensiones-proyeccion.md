# Dimensiones fijas de arreglo TLS C

El control C con `int[3]` e `int[2][3]` pasó, pero NSC los rechazó porque la
primera proyección solo aceptaba `int[2]`. Se recorren dimensiones fijas
positivas hasta el límite de 64 ya usado por el mapeo, con elemento `int`
mutable de 32 bits. El mapeo existente valida el tipo resultante. Otros tipos
y calificadores siguen pendientes; la construcción y los fixtures ampliados
esperan ejecución agrupada.
