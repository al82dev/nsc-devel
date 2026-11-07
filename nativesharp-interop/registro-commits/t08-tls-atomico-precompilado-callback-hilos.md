# T08: acceso NativeSharp al TLS atómico desde un segundo hilo

Un hilo creado por la biblioteca C invoca un callback NativeSharp publicado por el productor precompilado. El callback lee `_Thread_local _Atomic(int)` y después lo escribe; el código C del hilo contrasta los mismos valores. El hilo principal conserva su propio valor 5 tras la espera.

La cadena estática y la cadena con DLL NativeSharp intermedia pasaron O0–O3, con el encabezado C oculto al compilar los consumidores y herramientas de A:. Se verifican lectura y escritura para `int` en los dos hilos, no todos los tipos u operaciones. NSLink y los controles DLL originales no se modificaron; T08 continúa abierta.
