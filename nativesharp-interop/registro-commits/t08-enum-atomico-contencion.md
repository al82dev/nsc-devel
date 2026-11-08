# Tanda 8: actualizaciones cruzadas de enum atómico

Un hilo C y el hilo principal NativeSharp incrementan la misma global
`_Atomic(enum State)` 50 000 veces cada uno. Un evento confirma que el hilo C
está preparado antes de liberarlo; el resultado esperado tras esperar su fin es
100 000. El caso pasó O0–O3 y quedó incluido en la batería de la tanda 8.

Evidencia: `A:\nsc-interop-build\tanda08-atomic-enum-contencion-30872-30430`.
La prueba coordina los hilos pero no garantiza solapamiento físico de las
instrucciones ni sustituye una demostración exhaustiva de concurrencia. Una
primera ejecución detectó llaves ausentes en el `for` del fixture y se corrigió.
