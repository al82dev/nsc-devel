# T08-004: validación de DLL NativeSharp intermedia

Con el encabezado C oculto, una DLL NativeSharp enlazó el productor
precompilado y un consumidor la ejecutó en O0–O3. Se comprobó la presencia
de un índice TLS no nulo en cada DLL. Evidencia:
`A:\nsc-interop-build\tanda08-static-contratos-5753-27352`.

El primer intento falló por _tls_index y su validador devolvió cero por error.
Se declaró el CRT estático y se corrigió la salida del validador. Una copia
aislada con un identificador inválido falló con código uno y mostró el
diagnóstico; evidencia:
`A:\nsc-interop-build\tanda08-static-contratos-5632-22877`.

Esta prueba usa un hilo y no demuestra aislamiento TLS entre hilos ni el uso
de las declaraciones C dentro de plantillas públicas. La tanda sigue abierta.
