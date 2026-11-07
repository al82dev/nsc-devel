# T08: aislamiento TLS atómico con productor precompilado

El fixture añade un hilo creado desde la biblioteca C: comprueba que su instancia de `_Thread_local _Atomic(int)` empieza en 3 y la cambia a 71. El hilo consumidor NativeSharp había cambiado su propia instancia a 5 y verifica que permanece en 5 tras esperar al trabajador.

La prueba pasó O0–O3 tanto con el productor estático enlazado al ejecutable como con una DLL NativeSharp intermedia, usando el compilador de desarrollo de A:, Clang y LLD. El encabezado C se oculta al construir los consumidores. El trabajador accede desde C; no se afirma que NativeSharp accedió al TLS desde el segundo hilo. NSLink y las pruebas DLL originales permanecen intactos; la tanda 8 sigue abierta.
