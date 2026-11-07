# Tanda 8: prueba de selección con bitcode

La prueba compila una biblioteca C con TLS interno como ThinLTO y LTO completo. Comprueba que el IR contiene `thread_local` y que la biblioteca todavía no referencia `_tls_index`. El proyecto sin CRT debe seleccionar LLD y fallar por su ausencia; con el CRT necesario debe enlazar y verificar aislamiento entre hilos. Estos son controles de selección y ejecución, no una afirmación de cierre de la tanda.

Resultado: ambas variantes pasaron con el compilador de desarrollo construido en A:. Los controles existentes de TLS O0–O3, biblioteca sin TLS y selección explícita también pasaron.
