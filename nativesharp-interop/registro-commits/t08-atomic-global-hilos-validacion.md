# T08: global C atómico no TLS observado desde dos hilos

`pruebas-tanda-08-atomic-global/validar-hilos.cmd` se ejecutó en O0–O3. Un hilo
C y el hilo principal NativeSharp incrementaron el mismo global 10.000 veces
cada uno; ambos lenguajes observaron 20.000 tras esperar al hilo C. Los cuatro
ejecutables terminaron con código 0. El control no garantiza que los hilos se
solaparan ni demuestra todas las condiciones de concurrencia.

El control se incorporó al gate de la tanda, que aún no se ha repetido.
T08-004 y la tanda siguen abiertos; NSLink no se modificó.
