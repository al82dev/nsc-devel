# Comparaciones numéricas de enum C

Se amplía el caso con un enum C empaquetado con valor negativo y otro cuyo valor
requiere representación sin signo de 32 bits. La prueba comprueba igualdad y
orden en ambas direcciones, además del caso compartido. Falta validar O0–O3 y
comprobar que el enum NativeSharp conserve su comportamiento. La primera
ejecución devolvió 3 para el caso sin signo; se separaron igualdad y orden para
localizar la operación que falla. El IR mostró extensión con signo; Clang en
este destino había elegido `int` para el enum sin base explícita. La prueba
ahora especifica `unsigned int` y lo comprueba con `_Generic` en C. Esto usa
una extensión de Clang en C11; falta repetir la batería.
