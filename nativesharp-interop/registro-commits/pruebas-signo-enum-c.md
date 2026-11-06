# Comparaciones numéricas de enum C

Se amplía el caso con un enum C empaquetado con valor negativo y otro cuyo valor
requiere representación sin signo de 32 bits. La prueba comprueba igualdad y
orden en ambas direcciones, además del caso compartido. Falta validar O0–O3 y
comprobar que el enum NativeSharp conserve su comportamiento.
