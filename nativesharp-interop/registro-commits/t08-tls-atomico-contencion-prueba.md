# Contención sobre el objeto TLS atómico

El control C y el consumidor NativeSharp pasan la dirección del TLS del hilo
principal a un trabajador C mientras ese hilo permanece vivo. Ambos suman
10 000 veces y deben observar 20 000 al terminar. El caso comprueba que `+=`
sea indivisible, además de comparar las operaciones y el IR de la prueba.
Está preparado para validarse con el conjunto, no certifica todos los órdenes
de memoria o los demás tipos atómicos.
