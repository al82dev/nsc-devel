# T08: biblioteca precompilada que consume TLS C atómico

Se creó un productor NativeSharp que accede a `_Thread_local _Atomic(int)` y a punteros TLS atómicos a `const int` y `volatile int`, y un consumidor independiente de su biblioteca precompilada. El productor importa el header C; el consumidor solo usa la API NativeSharp publicada y se compila con ese header temporalmente oculto.

Con Clang 23.1.0, LLD y el compilador de desarrollo en A:, el productor y el consumidor se compilaron y ejecutaron en O0, O1, O2 y O3. El consumidor observó el valor inicial, una actualización atómica y accesos a los datos apuntados. La prueba se añadió al validador integral.

Esto verifica la operación dentro del productor precompilado y su dependencia empaquetada; no demuestra publicación directa de un global atómico, acceso entre hilos, todos los tipos, caché o DLL. No modifica NSLink ni cierra T08. Los controles DLL originales continúan abiertos.
